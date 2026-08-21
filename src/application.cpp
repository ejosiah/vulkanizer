#include <vulkanizer/application.hpp>

namespace vkz {

application::application(vulkan_app_create_info create_info)
        : name_{create_info.title ? create_info.title : "application"}
        , screen_size_{create_info.width, create_info.height}
        , app_{create_info}
        , context_{app_.context()}, device_{context_.device}, swapchain_{app_.create_swapchain()}
        , allocator_(vkz::vma_memory_allocator::create(context_))
        , image_views_{vkz::create_swapchain_image_views(device_, *swapchain_)} {

    const auto queue_family_index = app_.queue_family_index();
    const auto graphics_queue = app_.graphics_queue();
    auto *window = app_.window();

    vkz::imgui::init({
                             .window = window,
                             .vulkan_context = &context_,
                             .queue_family = queue_family_index,
                             .queue = graphics_queue,
                             .min_image_count = 2,
                             .image_count = swapchain_->image_count(),
                             .api_version = VK_API_VERSION_1_3,
                             .color_attachment_format = swapchain_->format(),
                     });

    commands_pool_ = std::make_unique<vkz::fenced_command_pools>(
            device_,
            graphics_queue,
            queue_family_index,
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            1
    );

    render_info_.color_attachments.push_back({
        .format = swapchain_->format(),
        .clear_value = {0.45f, 0.55f, 0.60f, 1.0f},
    });
    render_info_.render_area = {swapchain_->width(), swapchain_->height()};
}

application::application(const std::string &name, glm::uvec2 screen_size,
                         const device_extension_chain& extension_chain)
    : application(vulkan_app_create_info{
        .width = screen_size.x,
        .height = screen_size.y,
        .title = name.c_str(),
        .validation = true,
        .extension_chain = extension_chain,
    }) {
}

application::~application() {
    vkDeviceWaitIdle(device_.logical);
    commands_pool_.reset();
    vkz::imgui::destroy();
    vkz::destroy_image_views(device_.logical, image_views_);
    image_views_.clear();
    swapchain_.reset();
    allocator_.destroy();
}


void application::main_loop() {
    setup();

    ImGuiIO &io = ImGui::GetIO();

    auto &app = app_;
    auto &swapchain = swapchain_;
    auto &context = context_;
    auto &image_views = image_views_;
    auto *window = app.window();
    const auto graphics_queue = app.graphics_queue();

    VkSemaphore image_available = vkz::create_semaphore(device_.logical);
    VkSemaphore render_finished = vkz::create_semaphore(device_.logical);
    uint32_t frame{};

    while (!app_.should_close()) {
        app.poll_events();
        app.wait_for_drawable_window();
        if (app.should_close()) {
            break;
        }

        int framebuffer_width{};
        int framebuffer_height{};
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        if (static_cast<uint32_t>(framebuffer_width) != swapchain->width() ||
            static_cast<uint32_t>(framebuffer_height) != swapchain->height()) {
            recreate_swapchain();
            continue;
        }

        commands_pool_->set_cycle_and_wait(frame++);

        uint32_t image_index{};
        const auto acquire_result = vkAcquireNextImageKHR(
                context.device.logical,
                swapchain->handle(),
                UINT64_MAX,
                image_available,
                {},
                &image_index);

        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate_swapchain();
            continue;
        }
        VKZ_CHECK_VULKAN(acquire_result);

        vkz::imgui::new_frame();
        new_frame();

        auto command_buffer = commands_pool_->create_command_buffer();

        app_logic(command_buffer);

        auto image = swapchain->get_image(image_index);
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1;
        range.layerCount = 1;

        vkz::barrier::push_and_flush(
                command_buffer,
                image,
                range,
                VK_PIPELINE_STAGE_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_NONE,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        render_info_.color_attachments[0].view = image_views[image_index];

        vkz::render(command_buffer, render_info_, [&] {
            render_swapchain(command_buffer);
        });

        vkz::barrier::push_and_flush(
                command_buffer,
                image,
                range,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_2_NONE,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VKZ_CHECK_VULKAN(vkEndCommandBuffer(command_buffer));
        commands_pool_->enqueue(command_buffer);
        commands_pool_->enqueue_wait(image_available, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        commands_pool_->enqueue_signal(render_finished);
        VKZ_CHECK_VULKAN(commands_pool_->execute());

        VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        const auto swapchain_handle = swapchain->handle();
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &render_finished;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain_handle;
        present_info.pImageIndices = &image_index;

        const auto present_result = vkQueuePresentKHR(graphics_queue, &present_info);
        if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
            recreate_swapchain();
            continue;
        }
        VKZ_CHECK_VULKAN(present_result);
        end_frame();
    }
    vkDeviceWaitIdle(device_.logical);

    vkDestroySemaphore(device_.logical, render_finished, nullptr);
    vkDestroySemaphore(device_.logical, image_available, nullptr);
}

void application::render_swapchain(VkCommandBuffer command_buffer) {
    vkz::imgui::render(command_buffer);
}

void application::recreate_swapchain() {
    vkDeviceWaitIdle(device_.logical);
    app_.wait_for_drawable_window();
    if (app_.should_close()) {
        return;
    }

    vkz::destroy_image_views(device_.logical, image_views_);
    swapchain_.reset();
    swapchain_ = app_.create_swapchain();
    image_views_ = vkz::create_swapchain_image_views(device_, *swapchain_);
    render_info_.color_attachments[0].format = swapchain_->format();
    render_info_.render_area = {swapchain_->width(), swapchain_->height()};
}

void application::enqueue(VkCommandBuffer command_buffer) {
    commands_pool_->enqueue(command_buffer);
}

void application::enqueue(std::span<const VkCommandBuffer> command_buffers) {
    commands_pool_->enqueue(command_buffers.size(), command_buffers.data());
}

std::span<const VkCommandBuffer> application::allocate_command_buffers(std::size_t count) {
    const auto command_buffers = commands_pool_->create_command_buffers(VK_COMMAND_BUFFER_LEVEL_PRIMARY, count);
    return {command_buffers, count};
}

}
