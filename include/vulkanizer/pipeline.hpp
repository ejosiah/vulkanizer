#pragma once

#include "vkz.hpp"
#include "descriptors.hpp"
#include <volk.h>

namespace vkz {

    struct pipeline {
        using flags = vkz::pipeline_flags;
        // Preserve synchronization2 stage bits while retaining legacy values.
        enum class stage : VkPipelineStageFlags2 {
            none = VK_PIPELINE_STAGE_2_NONE,
            top_of_pipe = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            draw_indirect = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            vertex_input = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
            vertex_shader = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            tessellation_control_shader = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,
            tessellation_evaluation_shader = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,
            geometry_shader = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT,
            fragment_shader = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            early_fragment_tests = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            late_fragment_tests = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            color_attachment_output = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            compute_shader = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            transfer = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
            bottom_of_pipe = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            host = VK_PIPELINE_STAGE_2_HOST_BIT,
            all_graphics = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            all_commands = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            copy = VK_PIPELINE_STAGE_2_COPY_BIT,
            resolve = VK_PIPELINE_STAGE_2_RESOLVE_BIT,
            blit = VK_PIPELINE_STAGE_2_BLIT_BIT,
            clear = VK_PIPELINE_STAGE_2_CLEAR_BIT,
            index_input = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            vertex_attribute_input = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
            pre_rasterization_shaders = VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT,
            video_decode = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
            video_encode = VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR,
            transform_feedback = VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT,
            conditional_rendering = VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT,
            command_preprocess = VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV,
            fragment_shading_rate_attachment = VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
            acceleration_structure_build = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            ray_tracing_shader = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
            fragment_density_process = VK_PIPELINE_STAGE_2_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
            task_shader = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_NV,
            mesh_shader = VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_NV,
            subpass_shader = VK_PIPELINE_STAGE_2_SUBPASS_SHADER_BIT_HUAWEI,
            invocation_mask = VK_PIPELINE_STAGE_2_INVOCATION_MASK_BIT_HUAWEI,
            acceleration_structure_copy = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR,
            micromap_build = VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT,
            cluster_culling_shader = VK_PIPELINE_STAGE_2_CLUSTER_CULLING_SHADER_BIT_HUAWEI,
            optical_flow = VK_PIPELINE_STAGE_2_OPTICAL_FLOW_BIT_NV,
        };

        VkPipeline handle{};
        VkPipelineLayout layout{};
        VkPipelineBindPoint bind_point{VK_PIPELINE_BIND_POINT_COMPUTE};
        device device;

        std::vector<descriptor_set> descriptor_sets;

        void destroy() const {
            if (handle) {
                vkDestroyPipeline(device.logical, handle, nullptr);
            }
            if (layout) {
                vkDestroyPipelineLayout(device.logical, layout, nullptr);
            }
        }
    };

} // namespace vkz

template <> struct enable_bitmask_operators<vkz::pipeline::stage> {
    static constexpr bool enable = true;
};
