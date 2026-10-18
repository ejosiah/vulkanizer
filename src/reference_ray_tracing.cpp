#include <vulkanizer/reference.hpp>
#include <vulkanizer/ray_tracing_pipeline_builder.hpp>

namespace vkz::reference {
    std::array<VkDescriptorSetLayoutBinding, 7> ray_query_bindings(uint32_t texture_count) {
        VKZ_ASSERT(texture_count, "Ray queries require a texture descriptor");
        std::array<VkDescriptorSetLayoutBinding, 7> result;
        std::copy_n(ray_tracing_scene_bindings.begin(), result.size(), result.begin());
        for (auto &binding : result)
            binding.stageFlags = static_cast<VkShaderStageFlags>(vkz::shader_stage_fragment);
        result[6] = {6, static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), texture_count, static_cast<VkShaderStageFlags>(vkz::shader_stage_fragment), nullptr};
        return result;
    }

    VkDescriptorSetLayoutBinding ray_tracing_texture_binding(uint32_t texture_count) {
        VKZ_ASSERT(texture_count, "Reference ray tracing requires at least one texture descriptor");
        return {0, static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), texture_count, static_cast<VkShaderStageFlags>(ray_hit_stages), nullptr};
    }

    std::vector<ray_tracing_instance_uniform> make_ray_tracing_instances(const mesh::data &scene, uint32_t lod) {
        const auto stride = vertex_stride(scene);
        std::vector<ray_tracing_instance_uniform> result;
        result.reserve(scene.instances.size());
        for (const auto &instance : scene.instances) {
            const auto material = material_index(scene, instance);
            const auto &geometry = scene.meshes[instance.mesh_id];
            VKZ_ASSERT(geometry.lod_count <= mesh::max_lods && lod < geometry.lod_count, "Invalid reference ray tracing LOD");
            const auto begin = uint64_t(geometry.index_offset) + geometry.lod_offset[lod];
            const auto end = uint64_t(geometry.index_offset) + geometry.lod_offset[lod + 1];
            VKZ_ASSERT(end > begin && end <= scene.index_data.size() && (end - begin) % 3 == 0 && end <= UINT32_MAX, "Invalid reference triangle index range");
            VKZ_ASSERT((uint64_t(geometry.vertex_offset) + geometry.vertex_count) * stride * sizeof(float) <= scene.vertex_data.size(), "Reference mesh exceeds vertex data");
            result.push_back({{geometry.vertex_offset, static_cast<uint32_t>(begin), material, 0}});
        }
        return result;
    }

    vkz::ray_tracing_pipeline create_ray_tracing_pipeline(const ray_tracing_pipeline_create_info &info) {
        VKZ_ASSERT(info.layouts[0] && info.layouts[1] && info.texture_count, "Reference ray tracing requires scene and texture layouts");
        vkz::ray_tracing_pipeline_builder builder{info.allocator};
        builder
            .shader_groups()
                .ray_generation_group("primary")
                    .ray_generation_shader(shader(shader_stage::ray_generation))
                .miss_group("background")
                    .miss_shader(shader(shader_stage::miss, bool(info.ibl_layout)))
                .triangles_hit_group("surface")
                    .closest_hit_shader(shader(shader_stage::closest_hit, bool(info.ibl_layout)))
                    .add_specialization(info.texture_count, 0)
                    .any_hit_shader(shader(shader_stage::any_hit))
                    .add_specialization(info.texture_count, 0)
            .layout()
                .add_descriptor_set_layouts(info.layouts);
        if (info.ibl_layout) {
            builder.layout().add_descriptor_set_layout(info.ibl_layout);
        }
        return builder
            .max_recursion_depth(1)
            .name("gltf_core_ray_tracing_reference")
            .build();
    }
}
