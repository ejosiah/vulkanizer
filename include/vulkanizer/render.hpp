#pragma once

#include "memory.hpp"
#include "pipeline.hpp"

#include <volk.h>
#include <glm/glm.hpp>

#include <vector>
#include <optional>
#include <functional>

namespace vkz {

    using scene = std::function<void()>;

    struct color_attachment {
        image_view view;
        VkFormat format;
        glm::vec4 clear_value{0, 0, 0, 1};
        std::optional<image_view> resolve;
        bool clear{true};
    };

    struct depth_stencil_attachment {
        image_view view;
        VkFormat format;
        glm::vec2 clear_value{1, 0};
        bool clear{true};
    };

    struct render_info {
        std::vector<color_attachment> color_attachments;
        std::optional<depth_stencil_attachment> depth_attachment;
        std::optional<depth_stencil_attachment> stencil_attachment;
        glm::uvec2 render_area{};
        uint32_t num_layers{1};
        uint32_t view_mask{};
    };


    void render(VkCommandBuffer command_buffer, const render_info& render_info, scene scene);
}
