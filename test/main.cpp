#define VKZ_IOSTREAM_ADAPTER

#include <vulkanizer/barrier.hpp>
#include <vulkanizer/context.hpp>
#include <vulkanizer/imgui.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/render.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/swapchain.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <iostream>
#include <memory>
#include <vector>

namespace {
    constexpr uint32_t window_width = 1280;
    constexpr uint32_t window_height = 800;

    class glfw_surface_provider final : public vkz::surface_provider {
    public:
        explicit glfw_surface_provider(GLFWwindow* window)
            : _window{window} {
        }

        VkSurfaceKHR operator()(VkInstance instance) const override {
            VkSurfaceKHR surface{};
            VKZ_CHECK_VULKAN(glfwCreateWindowSurface(instance, _window, nullptr, &surface));
            return surface;
        }

    private:
        GLFWwindow* _window{};
    };

    uint32_t find_context_graphics_present_queue_family(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
        uint32_t queue_family_count{};
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

        for (uint32_t i = 0; i < queue_families.size(); ++i) {
            if (!(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                continue;
            }

            VkBool32 present_supported{};
            VKZ_CHECK_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_supported));

            if (!present_supported) {
                VKZ_THROW("The context graphics queue family does not support presentation")
            }

            return i;
        }

        VKZ_THROW("No graphics queue family is available")
    }

    VkCommandBuffer begin_command_buffer(VkDevice device, VkCommandPool command_pool) {
        VkCommandBufferAllocateInfo allocate_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.commandPool = command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer{};
        VKZ_CHECK_VULKAN(vkAllocateCommandBuffers(device, &allocate_info, &command_buffer));

        VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKZ_CHECK_VULKAN(vkBeginCommandBuffer(command_buffer, &begin_info));

        return command_buffer;
    }

    void submit_and_free(
            VkDevice device,
            VkQueue queue,
            VkCommandPool command_pool,
            VkCommandBuffer command_buffer,
            VkSemaphore wait_semaphore,
            VkSemaphore signal_semaphore,
            VkFence fence) {
        VKZ_CHECK_VULKAN(vkEndCommandBuffer(command_buffer));

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &wait_semaphore;
        submit_info.pWaitDstStageMask = &wait_stage;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &signal_semaphore;

        VKZ_CHECK_VULKAN(vkQueueSubmit(queue, 1, &submit_info, fence));
        VKZ_CHECK_VULKAN(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
        VKZ_CHECK_VULKAN(vkResetFences(device, 1, &fence));

        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    }

    std::vector<vkz::image_view> create_image_views(VkDevice device, vkz::swapchain& swapchain) {
        std::vector<vkz::image_view> image_views;
        image_views.reserve(swapchain.image_count());

        for (uint32_t i = 0; i < swapchain.image_count(); ++i) {
            VkImageViewCreateInfo create_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            create_info.image = swapchain.get_image(i);
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = swapchain.format();
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.layerCount = 1;

            VkImageView image_view{};
            VKZ_CHECK_VULKAN(vkCreateImageView(device, &create_info, nullptr, &image_view));
            image_views.push_back({
                    .handle = image_view,
                    .create_info = create_info,
            });
        }

        return image_views;
    }

    void destroy_image_views(VkDevice device, std::vector<vkz::image_view>& image_views) {
        for (auto image_view : image_views) {
            vkDestroyImageView(device, image_view.handle, nullptr);
        }

        image_views.clear();
    }

    void wait_for_drawable_window(GLFWwindow* window) {
        int width{};
        int height{};
        glfwGetFramebufferSize(window, &width, &height);

        while ((width == 0 || height == 0) && !glfwWindowShouldClose(window)) {
            glfwWaitEvents();
            glfwGetFramebufferSize(window, &width, &height);
        }
    }
}

int main() {
    vkz::iostream_adapter::install(std::cout);

    if (!glfwInit()) {
        VKZ_THROW("Failed to initialize GLFW")
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "vulkanizer context test", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        VKZ_THROW("Failed to create GLFW window")
    }

    uint32_t required_extension_count{};
    const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_extension_count);
    if (!required_extensions) {
        glfwDestroyWindow(window);
        glfwTerminate();
        VKZ_THROW("GLFW could not provide Vulkan instance extensions")
    }

    glfw_surface_provider surface_provider{window};
    VkPhysicalDeviceSynchronization2Features synchronization2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            nullptr,
            VK_TRUE,
    };
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            nullptr,
            VK_TRUE,
    };

    auto builder = vkz::context::builder();
    builder
            .app_name("vulkanizer context test")
            .engine_name("vulkanizer")
            .api_version(VK_API_VERSION_1_3)
            .surface(surface_provider)
            .add_extension(synchronization2)
            .add_extension(dynamic_rendering)
            .add_device_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    for (uint32_t i = 0; i < required_extension_count; ++i) {
        builder.add_instance_extension(required_extensions[i]);
    }

    auto context = builder.build();
    const auto surface = context.surface;

    const auto queue_family_index = find_context_graphics_present_queue_family(context.device.physical, surface);
    VkQueue graphics_queue{};
    vkGetDeviceQueue(context.device.logical, queue_family_index, 0, &graphics_queue);

    {
    auto create_swapchain = [&context] {
        return std::make_unique<vkz::swapchain>(
                vkz::swapchain::builder(context)
                        .set_image_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                        .build());
    };

    auto swapchain = create_swapchain();
    auto image_views = create_image_views(context.device.logical, *swapchain);

    vkz::imgui::init({
            .window = window,
            .vulkan_context = &context,
            .queue_family = queue_family_index,
            .queue = graphics_queue,
            .min_image_count = 2,
            .image_count = swapchain->image_count(),
            .api_version = VK_API_VERSION_1_3,
            .color_attachment_format = swapchain->format(),
    });

    VkCommandPoolCreateInfo command_pool_create_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    command_pool_create_info.queueFamilyIndex = queue_family_index;

    VkCommandPool command_pool{};
    VKZ_CHECK_VULKAN(vkCreateCommandPool(context.device.logical, &command_pool_create_info, nullptr, &command_pool));

    VkSemaphoreCreateInfo semaphore_create_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore image_available{};
    VkSemaphore render_finished{};
    VKZ_CHECK_VULKAN(vkCreateSemaphore(context.device.logical, &semaphore_create_info, nullptr, &image_available));
    VKZ_CHECK_VULKAN(vkCreateSemaphore(context.device.logical, &semaphore_create_info, nullptr, &render_finished));

    VkFenceCreateInfo fence_create_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence frame_fence{};
    VKZ_CHECK_VULKAN(vkCreateFence(context.device.logical, &fence_create_info, nullptr, &frame_fence));

    auto recreate_swapchain = [&] {
        vkDeviceWaitIdle(context.device.logical);
        wait_for_drawable_window(window);
        if (glfwWindowShouldClose(window)) {
            return;
        }

        destroy_image_views(context.device.logical, image_views);
        swapchain.reset();
        swapchain = create_swapchain();
        image_views = create_image_views(context.device.logical, *swapchain);
    };

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImGuiIO& io = ImGui::GetIO();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        wait_for_drawable_window(window);
        if (glfwWindowShouldClose(window)) {
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
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");
            ImGui::Text("This is some useful text.");
            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Checkbox("Another Window", &show_another_window);
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", reinterpret_cast<float*>(&clear_color));

            if (ImGui::Button("Button")) {
                counter++;
            }
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        if (show_another_window) {
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me")) {
                show_another_window = false;
            }
            ImGui::End();
        }

        auto command_buffer = begin_command_buffer(context.device.logical, command_pool);
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

        vkz::render_info render_info{};
        render_info.color_attachments.push_back({
                .view = image_views[image_index],
                .format = swapchain->format(),
                .clear_value = {clear_color.x, clear_color.y, clear_color.z, clear_color.w},
        });
        render_info.render_area = {swapchain->width(), swapchain->height()};

        vkz::render(command_buffer, render_info, [&] {
            vkz::imgui::render(command_buffer);
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

        submit_and_free(context.device.logical, graphics_queue, command_pool, command_buffer, image_available, render_finished,
                      frame_fence);

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
    }

    vkDeviceWaitIdle(context.device.logical);

    vkz::imgui::destroy();

    destroy_image_views(context.device.logical, image_views);
    swapchain.reset();

    vkDestroyFence(context.device.logical, frame_fence, nullptr);
    vkDestroySemaphore(context.device.logical, render_finished, nullptr);
    vkDestroySemaphore(context.device.logical, image_available, nullptr);
    vkDestroyCommandPool(context.device.logical, command_pool, nullptr);
    }

    context = {};

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
