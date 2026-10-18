#pragma once
#include "pipeline.hpp"
#include "ray_tracing_pipeline.hpp"
#include "mesh.hpp"
#include <glm/glm.hpp>
#include <array>
#include <vector>

namespace vkz::reference {
    enum class shader_stage {
        vertex,
        fragment,
        ray_generation,
        miss,
        closest_hit,
        any_hit,
        background_vertex,
        background_fragment,
        tone_mapping_fragment
    };

    [[nodiscard]] std::vector<uint32_t> shader(shader_stage stage, bool ibl = false, bool ray_query = false);

    struct ibl_uniform {
        glm::mat4 inverse_view_projection{1};
        glm::vec4 eye{};
        glm::vec4 parameters{1, 0, 0, 0};
    };

    static_assert(sizeof(ibl_uniform) == 96);

    [[nodiscard]] std::array<VkDescriptorSetLayoutBinding, 5> ibl_bindings(vkz::shader_stage stages);

    struct scene_uniform {
        glm::mat4 view_projection{1};
        glm::vec4 eye{};
        glm::vec4 light_direction{0, 1, 0, 0};
        glm::vec4 light_radiance{3, 3, 3, 0};
        glm::vec4 ambient_irradiance{0.15f, 0.15f, 0.15f, 0};
        glm::ivec4 vertex_layout{};
    };

    struct light_uniform {
        glm::vec4 position_range{};
        glm::vec4 direction_type{};
        glm::vec4 radiance{};
        glm::vec4 cone{};
    };

    static_assert(sizeof(light_uniform) == 64);
    [[nodiscard]] std::vector<light_uniform> make_light_uniforms(const mesh::data &scene);

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

    static_assert(sizeof(scene_uniform) == 144 && sizeof(material_uniform) == 992 && sizeof(instance_uniform) == 128);

    inline constexpr std::array<VkDescriptorSetLayoutBinding, 3> scene_bindings{
        {{0, static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_vertex | vkz::shader_stage_fragment), nullptr},
         {1, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_vertex), nullptr},
         {2, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_fragment), nullptr}}};
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

    struct ray_tracing_scene_uniform {
        scene_uniform scene;
        glm::mat4 inverse_view_projection{1};
        glm::vec4 background{0, 0, 0, 1};
        glm::vec4 ray_parameters{0.001f, 1000, 0, 0};
        glm::uvec4 sampling{0, 0xff, 8, 8};
    };

    struct ray_tracing_instance_uniform {
        glm::uvec4 geometry{};
    };

    static_assert(sizeof(ray_tracing_scene_uniform) == 256 && sizeof(ray_tracing_instance_uniform) == 16);

    inline constexpr vkz::shader_stage ray_hit_stages = vkz::shader_stage_closest_hit | vkz::shader_stage_any_hit;
    inline constexpr std::array<VkDescriptorSetLayoutBinding, 8> ray_tracing_scene_bindings{
        {{0, static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_raygen | vkz::shader_stage_miss | ray_hit_stages), nullptr},
         {1, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 1, static_cast<VkShaderStageFlags>(ray_hit_stages), nullptr},
         {2, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 1, static_cast<VkShaderStageFlags>(ray_hit_stages), nullptr},
         {3, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 1, static_cast<VkShaderStageFlags>(ray_hit_stages), nullptr},
         {4, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 1, static_cast<VkShaderStageFlags>(ray_hit_stages), nullptr},
         {5, static_cast<VkDescriptorType>(vkz::descriptor_type_acceleration_structure), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_raygen), nullptr},
         {6, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_raygen), nullptr},
         {7, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 1, static_cast<VkShaderStageFlags>(ray_hit_stages), nullptr}}};

    [[nodiscard]] VkDescriptorSetLayoutBinding ray_tracing_texture_binding(uint32_t texture_count);
    [[nodiscard]] std::vector<ray_tracing_instance_uniform> make_ray_tracing_instances(const mesh::data &scene, uint32_t lod = 0);

    struct ray_tracing_pipeline_create_info {
        vma_memory_allocator &allocator;
        std::array<descriptor_set_layout, 2> layouts;
        uint32_t texture_count{1};
        descriptor_set_layout ibl_layout{};
    };

    [[nodiscard]] vkz::ray_tracing_pipeline create_ray_tracing_pipeline(const ray_tracing_pipeline_create_info &info);

    [[nodiscard]] std::array<VkDescriptorSetLayoutBinding, 7> ray_query_bindings(uint32_t texture_count);

    struct pipeline_create_info {
        vkz::device device;

        std::array<descriptor_set_layout, 2> layouts;
        VkExtent2D extent{};
        VkFormat color_format{VK_FORMAT_UNDEFINED};
        VkFormat depth_format{VK_FORMAT_UNDEFINED};
        vkz::sample_count samples{vkz::sample_count_x1};
        bool alpha_blend{};
        bool mirrored{};
        descriptor_set_layout ibl_layout{};
        descriptor_set_layout ray_query_layout{};
        uint32_t texture_count{1};
    };

    struct background_pipeline_create_info {
        vkz::device device;
        descriptor_set_layout ibl_layout;
        VkExtent2D extent{};
        VkFormat color_format{VK_FORMAT_UNDEFINED};
        VkFormat depth_format{VK_FORMAT_UNDEFINED};
        vkz::sample_count samples{vkz::sample_count_x1};
    };

    inline constexpr VkDescriptorSetLayoutBinding tone_mapping_binding{0, static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 1,
                                                                       static_cast<VkShaderStageFlags>(vkz::shader_stage_fragment), nullptr};

    struct tone_mapping_uniform {
        float exposure{};
    };

    struct tone_mapping_pipeline_create_info {
        vkz::device device;
        descriptor_set_layout layout;
        VkExtent2D extent{};
        VkFormat color_format{VK_FORMAT_UNDEFINED};
    };

    [[nodiscard]] vkz::pipeline create_tone_mapping_pipeline(const tone_mapping_pipeline_create_info &info);
    [[nodiscard]] vkz::pipeline create_background_pipeline(const background_pipeline_create_info &info);
    [[nodiscard]] vkz::pipeline create_pipeline(const pipeline_create_info &info);
}
