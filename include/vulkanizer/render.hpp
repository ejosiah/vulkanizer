#pragma once

#include "memory.hpp"

#include <volk.h>
#include <glm/glm.hpp>

#include <vector>
#include <optional>
#include <functional>

namespace vkz {

    using scene = std::function<void()>;

    struct pipeline {
        VkPipeline handle{};
        VkPipelineLayout layout{};
    };

    struct color_attachment {
        ImageView imageView;
        VkFormat format;
        glm::vec4 clearValue{0, 0, 0, 1};
        std::optional<ImageView> resolve;
        bool clear{true};
    };

    struct depth_stencil_attachment {
        ImageView imageView;
        VkFormat format;
        glm::vec2 clearValue{1, 0};
        bool clear{true};
    };

    struct render_info {
        std::vector<color_attachment> colorAttachments;
        std::optional<depth_stencil_attachment> depthAttachment;
        std::optional<depth_stencil_attachment> stencilAttachment;
        glm::uvec2 renderArea{};
        uint32_t numLayers{1};
        uint32_t viewMask{};
    };


    void render(VkCommandBuffer commandBuffer, const render_info& renderInfo, scene scene);
}