#pragma once

#include "context.hpp"
#include "memory.hpp"
#include "texture.hpp"
#include "pipeline.hpp"

#include <glm/glm.hpp>

#include <functional>


namespace vkz::psm {

    static constexpr uint32_t DEFAULT_SHADOW_MAP_SIZE = 1024;
    static constexpr float DEFAULT_RANGE = 1024.0f;
    using scene = std::function<void(vkz::pipeline)>;

    using id = uint32_t;

    struct params {
        vkz::device device;
        VkFormat depth_format{};
        vma_memory_allocator memory_allocator;
        std::string vertex_shader_include;  // should contain get_model_matrix()
        VkDeviceSize vertex_shader_position_offset{};
        VkDeviceSize vertex_shader_position_stride{sizeof(glm::vec3)};
        descriptor_set_layout vertex_include_descriptor_set_layout{};
        descriptor_pool descriptor_pool;
        glm::uvec2 resolution{DEFAULT_SHADOW_MAP_SIZE};
        uint32 in_flight_frames{1};
        uint32 queue_family_index{};
        float range{DEFAULT_RANGE};
    };

    id create(const params& params);

    void destroy(id id);

    void update(id id, const glm::vec3& light_position);

    void capture(id id, VkCommandBuffer command_buffer, uint32 frame_index, const scene& scene);

    texture get_shadow_map(id id, uint32 frame_index);

}
