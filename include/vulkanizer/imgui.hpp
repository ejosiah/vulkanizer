#pragma once

#include "context.hpp"

struct GLFWwindow;

namespace vkz::imgui {

    struct params {
        GLFWwindow* window{};
        const context* vulkanContext{};
        uint32_t queueFamily{};
        VkQueue queue{};
        uint32_t minImageCount{2};
        uint32_t imageCount{2};
        uint32_t apiVersion{VK_API_VERSION_1_3};
        VkFormat colorAttachmentFormat{VK_FORMAT_UNDEFINED};
        VkRenderPass renderPass{};
        VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
        bool installCallbacks{true};
        bool enableKeyboardNavigation{true};
        bool enableGamepadNavigation{};
        bool useDynamicRendering{true};
        uint32_t descriptorPoolSize{32};
        VkDescriptorPool descriptorPool{};
        VkPipelineCache pipelineCache{};
        const VkAllocationCallbacks* allocator{};
    };

    void init(const params& params);

    void newFrame();

    void render(VkCommandBuffer commandBuffer);

    void destroy();

}
