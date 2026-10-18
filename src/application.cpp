#include <vulkanizer/application.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace vkz {

    namespace {
        vulkan_app_create_info validate_frames(vulkan_app_create_info info, uint32_t count) {
            if (!count) throw std::invalid_argument("Frames in flight must be greater than zero");
            return info;
        }
    }

    application::application(vulkan_app_create_info create_info, uint32_t frames_in_flight)
            : frames_in_flight_{frames_in_flight}, depth_testing_{create_info.depth_testing}, name_{create_info.title ? create_info.title : "application"},
              screen_size_{create_info.width, create_info.height}, app_{validate_frames(create_info, frames_in_flight)}, context_{app_.context()},
              device_{context_.device}, swapchain_{app_.create_swapchain()},
              allocator_(vkz::vma_memory_allocator::create(context_)),
              image_views_{vkz::create_swapchain_image_views(device_, *swapchain_)} {

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
                                 .depth_attachment_format = create_info.depth_testing ? pick_depth_format(device_) : VK_FORMAT_UNDEFINED,
                         });

        commands_pool_ = std::make_unique<vkz::fenced_command_pools>(
                device_,
                graphics_queue,
                queue_family_index,
                vkz::command_pool_flags_transient,
                frames_in_flight_
        );

        render_info_.color_attachments.push_back({
             .format = swapchain_->format(),
             .clear_value = {0.45f, 0.55f, 0.60f, 1.0f},
         });

        image_available_.resize(frames_in_flight_);
        for (auto &semaphore : image_available_) semaphore = vkz::create_semaphore(device_.logical);
        create_frame_attachments();

        render_info_.render_area = {swapchain_->width(), swapchain_->height()};
    }

    application::application(const std::string &name, glm::uvec2 screen_size,
                             const device_extension_chain &extension_chain, uint32_t frames_in_flight)
            : application(vulkan_app_create_info{
            .width = screen_size.x,
            .height = screen_size.y,
            .title = name.c_str(),
            .validation = true,
            .extension_chain = extension_chain,
    }, frames_in_flight) {
    }

    application::~application() {
        vkDeviceWaitIdle(device_.logical);
        commands_pool_.reset();
        vkz::imgui::destroy();
        vkz::destroy_image_views(device_.logical, image_views_);
        image_views_.clear();
        destroy_frame_attachments();
        for (auto semaphore : image_available_) vkDestroySemaphore(device_.logical, semaphore, nullptr);
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

        frame_index_ = 0;

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

            commands_pool_->set_cycle_and_wait(frame_index_);
            const auto image_available = image_available_[frame_index_];

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
            if (acquire_result != VK_SUBOPTIMAL_KHR) VKZ_CHECK_VULKAN(acquire_result);
            const auto render_finished = render_finished_[image_index];
            if (depth_testing_) {
                depth_buffer_ = depth_buffers_[frame_index_];
                render_info_.depth_attachment->view = depth_buffer_->image_view;
            }

            vkz::imgui::new_frame();
            new_frame();

            auto command_buffer = commands_pool_->create_command_buffer();

            if (depth_testing_) {
                auto &depth = depth_buffers_[frame_index_];
                VkImageSubresourceRange depth_range{static_cast<VkImageAspectFlags>(vkz::image_aspect_depth), 0, 1, 0, 1};
                const auto stages = vkz::pipeline_stage_early_fragment_tests | vkz::pipeline_stage_late_fragment_tests;
                vkz::barrier::push_and_flush(command_buffer, depth.image.handle, depth_range, depth_initialized_[frame_index_] ? stages : vkz::pipeline_stage_none, stages,
                                             depth_initialized_[frame_index_] ? vkz::access_depth_stencil_attachment_write : vkz::access_none, vkz::access_depth_stencil_attachment_read_write,
                                             depth_initialized_[frame_index_] ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
                depth_initialized_[frame_index_] = true;
                depth.image.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                depth_buffer_->image.layout = depth.image.layout;
            }
            app_logic(command_buffer);

            auto image = swapchain->get_image(image_index);
            VkImageSubresourceRange range{};
            range.aspectMask = static_cast<VkImageAspectFlags>(vkz::image_aspect_color);
            range.levelCount = 1;
            range.layerCount = 1;

            vkz::barrier::push_and_flush(command_buffer, image, range,
                                         // Chain the layout transition after the acquire semaphore's wait stage.
                                         vkz::pipeline_stage_color_attachment_output, vkz::pipeline_stage_color_attachment_output, vkz::access_none, vkz::access_color_attachment_write,
                                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            render_info_.color_attachments[0].view = image_views[image_index];

            vkz::render(command_buffer, render_info_, [&] {
                render_swapchain(command_buffer);
            });

            vkz::barrier::push_and_flush(command_buffer, image, range, vkz::pipeline_stage_color_attachment_output, vkz::pipeline_stage_none, vkz::access_color_attachment_write,
                                         vkz::access_none, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            VKZ_CHECK_VULKAN(vkEndCommandBuffer(command_buffer));
            commands_pool_->enqueue(command_buffer);
            commands_pool_->enqueue_wait(image_available, vkz::pipeline_stage_color_attachment_output);
            commands_pool_->enqueue_signal(render_finished);
            VKZ_CHECK_VULKAN(commands_pool_->execute());
            frame_submitted();

            VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            const auto swapchain_handle = swapchain->handle();
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = &render_finished;
            present_info.swapchainCount = 1;
            present_info.pSwapchains = &swapchain_handle;
            present_info.pImageIndices = &image_index;

            const auto present_result = vkQueuePresentKHR(graphics_queue, &present_info);
            end_frame();
            frame_index_ = (frame_index_ + 1) % frames_in_flight_;
            if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR || acquire_result == VK_SUBOPTIMAL_KHR) {
                if (present_result != VK_ERROR_OUT_OF_DATE_KHR && present_result != VK_SUBOPTIMAL_KHR) VKZ_CHECK_VULKAN(present_result);
                recreate_swapchain();
                continue;
            }
            VKZ_CHECK_VULKAN(present_result);
        }
        vkDeviceWaitIdle(device_.logical);

    }

    void application::render_swapchain(VkCommandBuffer command_buffer) {
        vkz::imgui::render(command_buffer);
    }

    void application::recreate_swapchain() {
        vkz::warn("Swapchain invalidated; recreating it");
        vkDeviceWaitIdle(device_.logical);
        app_.wait_for_drawable_window();
        if (app_.should_close()) {
            return;
        }

        destroy_frame_attachments();
        vkz::destroy_image_views(device_.logical, image_views_);
        swapchain_.reset();
        swapchain_ = app_.create_swapchain();
        image_views_ = vkz::create_swapchain_image_views(device_, *swapchain_);
        render_info_.color_attachments[0].format = swapchain_->format();
        render_info_.render_area = {swapchain_->width(), swapchain_->height()};
        create_frame_attachments();
    }

    void application::create_frame_attachments() {
        render_finished_.resize(swapchain_->image_count());
        for (auto &semaphore : render_finished_) semaphore = vkz::create_semaphore(device_.logical);
        if (!depth_testing_) return;
        const auto format = pick_depth_format(device_);
        depth_buffers_.resize(frames_in_flight_);
        depth_initialized_.assign(frames_in_flight_, false);
        for (auto &depth : depth_buffers_) {
            depth.image = image::builder(allocator_)
                .format(format)
                .type(VK_IMAGE_TYPE_2D)
                .extent(swapchain_->width(), swapchain_->height())
                .usage(vkz::image_usage_depth_stencil)
                .build();
            depth.image_view = image_view::builder(device_)
                .image(depth.image)
                .format(format)
                .view_type(VK_IMAGE_VIEW_TYPE_2D)
                .aspect_mask(vkz::image_aspect_depth)
                .build();
        }
        render_info_.depth_attachment = {.view = depth_buffers_[frame_index_].image_view, .format = format};
        depth_buffer_ = depth_buffers_[frame_index_];
    }

    void application::destroy_frame_attachments() {
        for (auto semaphore : render_finished_) vkDestroySemaphore(device_.logical, semaphore, nullptr);
        render_finished_.clear();
        depth_buffer_.reset();
        for (auto &depth : depth_buffers_) depth.destroy();
        depth_buffers_.clear();
        depth_initialized_.clear();
        render_info_.depth_attachment.reset();
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

    VkFormat application::color_format() const {
        return swapchain_->format();
    }

    VkFormat application::depth_format() const {
        return render_info_.depth_attachment ? render_info_.depth_attachment->format : VK_FORMAT_UNDEFINED;
    }

}
