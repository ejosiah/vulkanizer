#define VKZ_IOSTREAM_ADAPTER

#include <vulkanizer/vulkan_app.hpp>

#include <vulkanizer/barrier.hpp>
#include <vulkanizer/imgui.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/render.hpp>
#include <vulkanizer/status.hpp>

#include <imgui.h>

#include <iostream>

namespace {
    constexpr uint32_t window_width = 1280;
    constexpr uint32_t window_height = 800;
}

int main() {
    vkz::iostream_adapter::install(std::cout);

    vkz::vulkan_app app{{window_width, window_height, "vulkanizer context test"}};
    auto& context = app.context();
    const auto queue_family_index = app.queue_family_index();
    const auto graphics_queue = app.graphics_queue();
    auto* window = app.window();

    {
    auto swapchain = app.create_swapchain();
    auto image_views = vkz::create_swapchain_image_views(context.device.logical, *swapchain);

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

    VkCommandPool command_pool = vkz::create_command_pool(context.device.logical, queue_family_index);
    VkSemaphore image_available = vkz::create_semaphore(context.device.logical);
    VkSemaphore render_finished = vkz::create_semaphore(context.device.logical);
    VkFence frame_fence = vkz::create_fence(context.device.logical);

    auto recreate_swapchain = [&] {
        vkDeviceWaitIdle(context.device.logical);
        app.wait_for_drawable_window();
        if (app.should_close()) {
            return;
        }

        vkz::destroy_image_views(context.device.logical, image_views);
        swapchain.reset();
        swapchain = app.create_swapchain();
        image_views = vkz::create_swapchain_image_views(context.device.logical, *swapchain);
    };

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImGuiIO& io = ImGui::GetIO();

    while (!app.should_close()) {
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

        auto command_buffer = vkz::begin_command_buffer(context.device.logical, command_pool);
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

        vkz::submit_and_free(context.device.logical, graphics_queue, command_pool, command_buffer, image_available, render_finished,
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

    vkz::destroy_image_views(context.device.logical, image_views);
    swapchain.reset();

    vkDestroyFence(context.device.logical, frame_fence, nullptr);
    vkDestroySemaphore(context.device.logical, render_finished, nullptr);
    vkDestroySemaphore(context.device.logical, image_available, nullptr);
    vkDestroyCommandPool(context.device.logical, command_pool, nullptr);
    }

    return 0;
}
