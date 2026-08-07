#pragma once

#include <vulkanizer/context.hpp>
#include <vulkanizer/memory.hpp>
#include <vulkanizer/swapchain.hpp>
#include <vulkanizer/camera/controller.hpp>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <memory>
#include <vector>
#include <optional>

namespace vkz {

    struct vulkan_app_create_info {
        uint32_t width{};
        uint32_t height{};
        const char* title{};
        bool synchronization2{true};
        bool dynamic_rendering{true};
        bool multiview{};
        bool resizable{true};
    };

    class glfw_surface_provider final : public surface_provider {
    public:
        explicit glfw_surface_provider(GLFWwindow* window);

        VkSurfaceKHR operator()(VkInstance instance) const override;

    private:
        GLFWwindow* window_{};
    };

    class glfw_runtime {
    public:
        glfw_runtime();
        ~glfw_runtime();

        glfw_runtime(const glfw_runtime&) = delete;
        glfw_runtime& operator=(const glfw_runtime&) = delete;
    };

    struct glfw_window_deleter {
        void operator()(GLFWwindow* window) const;
    };

    class vulkan_app {
    public:
        explicit vulkan_app(const vulkan_app_create_info& create_info);
        ~vulkan_app();

        vulkan_app(const vulkan_app&) = delete;
        vulkan_app& operator=(const vulkan_app&) = delete;
        vulkan_app(vulkan_app&&) = delete;
        vulkan_app& operator=(vulkan_app&&) = delete;

        [[nodiscard]] GLFWwindow* window() const;
        [[nodiscard]] vkz::context& context();
        [[nodiscard]] const vkz::context& context() const;
        [[nodiscard]] uint32_t queue_family_index() const;
        [[nodiscard]] VkQueue graphics_queue() const;
        [[nodiscard]] bool should_close() const;

        void poll_events() const;
        void wait_for_drawable_window() const;

        [[nodiscard]]
        std::unique_ptr<vkz::swapchain> create_swapchain(VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) const;

    private:
        glfw_runtime runtime_;
        std::unique_ptr<GLFWwindow, glfw_window_deleter> window_;
        glfw_surface_provider surface_provider_;
        vkz::context context_;
        uint32_t queue_family_index_{};
        VkQueue graphics_queue_{};
    };

    uint32_t find_graphics_present_queue_family(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

    VkCommandPool create_command_pool(
        VkDevice device,
        uint32_t queue_family_index,
        VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    VkCommandBuffer begin_command_buffer(VkDevice device, VkCommandPool command_pool);

    void submit_and_free(
        VkDevice device,
        VkQueue queue,
        VkCommandPool command_pool,
        VkCommandBuffer command_buffer,
        VkSemaphore wait_semaphore = {},
        VkSemaphore signal_semaphore = {},
        VkFence fence = {});

    VkSemaphore create_semaphore(VkDevice device);
    VkFence create_fence(VkDevice device);

    VkFormat pick_depth_format(VkPhysicalDevice physical_device);

    std::vector<vkz::image_view> create_swapchain_image_views(VkDevice device, vkz::swapchain& swapchain);
    void destroy_image_views(VkDevice device, std::vector<vkz::image_view>& image_views);

}
