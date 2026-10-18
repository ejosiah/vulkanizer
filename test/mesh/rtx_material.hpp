#pragma once

#include <vulkanizer/shader_stage.hpp>
#include <vulkanizer/descriptor.hpp>
#include <vulkanizer/mesh.hpp>
#include <array>
#include <span>

namespace vkz::rtx {
    struct scene_uniform {
        glm::mat4 view_projection{1};
        glm::vec4 eye{};
        glm::ivec4 vertex_layout{};
        uint32_t num_lights{};
        uint32_t enable_ddgi{1};
        uint32_t indirect_only{};
        float exposure{1};
        float gi_multiplier{1};
        uint32_t enable_shadows{1};
        glm::vec2 render_resolution{};
        uint32_t enable_reflection{1};
        uint32_t reflection_only{};
    };

    struct light_uniform {
        glm::vec4 position_range{};
        glm::vec4 direction_type{};
        glm::vec4 radiance{};
        glm::vec4 cone{};
    };

    static_assert(sizeof(light_uniform) == 64);
    [[nodiscard]] std::vector<light_uniform> make_light_uniforms(std::span<const mesh::light> lights);

    struct material_uniform {
        glm::vec4 base_color{1};
        glm::vec4 emissive_metallic{0, 0, 0, 1};
        glm::vec4 roughness_normal_occlusion_cutoff{1, 1, 1, 0.5f};
        glm::ivec4 flags{};
        glm::vec4 optical{1.5f, 0, 0, 0};
        glm::vec4 attenuation{1, 1, 1, std::numeric_limits<float>::infinity()};
        glm::vec4 specular{1};
        glm::vec4 clearcoat{0, 0, 1, 0};
        glm::vec4 sheen{};
        glm::vec4 anisotropy{0, 1, 0, 0};
        glm::vec4 iridescence{0, 1.3f, 100, 400};
        std::array<glm::ivec4, mesh::material_texture_count> textures{};
        std::array<glm::vec4, mesh::material_texture_count> uv_transform{};
        std::array<glm::vec4, mesh::material_texture_count> uv_offset{};
    };

    struct instance_uniform {
        glm::mat4 model{1}, normal_matrix{1};
    };

    static_assert(sizeof(scene_uniform) == 136 && sizeof(material_uniform) == 992 && sizeof(instance_uniform) == 128);

    inline constexpr std::array<VkDescriptorSetLayoutBinding, 5> scene_bindings{
        {{0, static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_vertex | vkz::shader_stage_fragment), nullptr},
         {1, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_vertex), nullptr},
         {2, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_fragment), nullptr},
         {3, static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_fragment), nullptr},
         {4, static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_fragment), nullptr}}};

    inline constexpr auto material_bindings = [] {
        std::array<VkDescriptorSetLayoutBinding, mesh::material_texture_count + 1> result{};
        result[0] = {0, static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_vertex | vkz::shader_stage_fragment), nullptr};
        for (uint32_t i = 1; i < result.size(); ++i)
            result[i] = {i, static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_fragment), nullptr};
        return result;
    }();

    [[nodiscard]] uint32_t vertex_stride(const mesh::data &scene);
    [[nodiscard]] uint32_t material_index(const mesh::data &scene, const mesh::instance &instance);
    [[nodiscard]] material_uniform make_material_uniform(const mesh::data &scene, const mesh::material &material);

}


