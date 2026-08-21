#pragma once


#include "types.hpp"
#include "status.hpp"
#include <volk.h>

namespace vkz {

    struct device {

        struct features;

        VkPhysicalDevice physical{};
        VkDevice logical{};

        operator VkDevice() const {
            return logical;
        }

        operator VkPhysicalDevice() const {
            return physical;
        }

        operator bool() const {
            return logical != VK_NULL_HANDLE;
        }
    };

    struct shader_info{
        VkShaderModule module{};
        VkShaderStageFlagBits stage{};
        const char* entry{"main"};
    };

    template<VkObjectType object_type>
    inline void set_name(vkz::device device, const std::string& object_name, void* ptr)  {
#ifndef NDEBUG
        auto object_handle = (uint64_t)ptr;
        VkDebugUtilsObjectNameInfoEXT name_info{};
        name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        name_info.pObjectName = object_name.c_str();
        name_info.objectType = object_type;
        name_info.objectHandle = object_handle;
        vkSetDebugUtilsObjectNameEXT(device.logical, &name_info);
#endif
    }

    struct device::features {
        bool robust_buffer_access{};
        bool full_draw_index_uint32{};
        bool image_cube_array{};
        bool independent_blend{};
        bool geometry_shader{};
        bool tessellation_shader{};
        bool sample_rate_shading{};
        bool dual_src_blend{};
        bool logic_op{};
        bool multi_draw_indirect{};
        bool draw_indirect_first_instance{};
        bool depth_clamp{};
        bool depth_bias_clamp{};
        bool fill_mode_non_solid{};
        bool depth_bounds{};
        bool wide_lines{};
        bool large_points{};
        bool alpha_to_one{};
        bool multi_viewport{};
        bool sampler_anisotropy{};
        bool texture_compression_etc2{};
        bool texture_compression_astc_ldr{};
        bool texture_compression_bc{};
        bool occlusion_query_precise{};
        bool pipeline_statistics_query{};
        bool vertex_pipeline_stores_and_atomics{};
        bool fragment_stores_and_atomics{};
        bool shader_tessellation_and_geometry_point_size{};
        bool shader_image_gather_extended{};
        bool shader_storage_image_extended_formats{};
        bool shader_storage_image_multisample{};
        bool shader_storage_image_read_without_format{};
        bool shader_storage_image_write_without_format{};
        bool shader_uniform_buffer_array_dynamic_indexing{};
        bool shader_sampled_image_array_dynamic_indexing{};
        bool shader_storage_buffer_array_dynamic_indexing{};
        bool shader_storage_image_array_dynamic_indexing{};
        bool shader_clip_distance{};
        bool shader_cull_distance{};
        bool shader_float64{};
        bool shader_int64{};
        bool shader_int16{};
        bool shader_resource_residency{};
        bool shader_resource_min_lod{};
        bool sparse_binding{};
        bool sparse_residency_buffer{};
        bool sparse_residency_image_2d{};
        bool sparse_residency_image_3d{};
        bool sparse_residency_2_samples{};
        bool sparse_residency_4_samples{};
        bool sparse_residency_8_samples{};
        bool sparse_residency_16_samples{};
        bool sparse_residency_aliased{};
        bool variable_multisample_rate{};
        bool inherited_queries{};
    };
}
