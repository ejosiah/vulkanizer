#pragma once

#include "context.hpp"

struct GLFWwindow;

namespace vkz::imgui {

    struct params {
        GLFWwindow* window{};
        const context* vulkan_context{};
        uint32_t queue_family{};
        VkQueue queue{};
        uint32_t min_image_count{2};
        uint32_t image_count{2};
        uint32_t api_version{VK_API_VERSION_1_3};
        VkFormat color_attachment_format{VK_FORMAT_UNDEFINED};
        VkRenderPass render_pass{};
        VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
        bool install_callbacks{true};
        bool enable_keyboard_navigation{true};
        bool enable_gamepad_navigation{};
        bool use_dynamic_rendering{true};
        uint32_t descriptor_pool_size{32};
        VkDescriptorPool descriptor_pool{};
        VkPipelineCache pipeline_cache{};
        const VkAllocationCallbacks* allocator{};
    };

    void init(const params& params);

    void new_frame();

    void render(VkCommandBuffer command_buffer);

    void destroy();

}
