#pragma once

#include "context.hpp"
#include "memory.hpp"

#include <glm/glm.hpp>
#include <span>
#include <string>
#include <functional>


namespace vkz::csm {

    static constexpr uint32_t DEFAULT_SHADOW_MAP_SIZE = 4096;
    static constexpr uint32_t DEFAULT_CASCADE_COUNT = 4;
    static constexpr float DEFAULT_CASCADE_SLIT_LAMBDA = 0.95f;
    using scene = std::function<void(VkPipelineLayout)>;
    using id = uint32_t;

    struct params {
        context context;
        vma_memory_allocator memory_allocator;
        std::string vertex_shader_include;  // should contain get_model_matrix()
        VkDeviceSize vertex_shader_position_offset{};
        VkDeviceSize vertex_shader_position_stride{sizeof(glm::vec3)};
        VkDescriptorSetLayout vertex_include_descriptorset_layout{};
        VkDescriptorPool descriptor_pool{};
        uint in_flight_frames{};
        uint num_cascades{DEFAULT_CASCADE_COUNT};
        uint size{DEFAULT_SHADOW_MAP_SIZE};
    };

    struct camera {
        glm::mat4 view_projection{1};
        float near_plane{0.1};
        float far_plane{1000};
    };

    id create(const params& params);

    void destroy(id id);

    void update(id id, const camera& camera, const glm::vec3& light_direction, std::span<float> split_depth);

    void capture(id id, const scene& scene, VkCommandBuffer command_buffer, int current_frame);

    const texture shadow_map(id id, int index);

    uint cascade_count(id id);

    buffer cascade_view_projection(id id);

    VkDescriptorSetLayout descriptor_set_layout(id id);

    VkDescriptorSet descriptor_set(id id);

    void set(id id, VkRenderPass render_pass, glm::uvec2 resolution);

    void render(id id, VkCommandBuffer command_buffer);

    void split_lambda(id id, float value);
}
