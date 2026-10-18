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
    // Flat aliases retain the original enum type and bitmask operators.
    using pipeline_stage = pipeline::stage;
    inline constexpr pipeline_stage pipeline_stage_none = pipeline::stage::none;
    inline constexpr pipeline_stage pipeline_stage_top_of_pipe = pipeline::stage::top_of_pipe;
    inline constexpr pipeline_stage pipeline_stage_draw_indirect = pipeline::stage::draw_indirect;
    inline constexpr pipeline_stage pipeline_stage_vertex_input = pipeline::stage::vertex_input;
    inline constexpr pipeline_stage pipeline_stage_vertex_shader = pipeline::stage::vertex_shader;
    inline constexpr pipeline_stage pipeline_stage_tessellation_control_shader = pipeline::stage::tessellation_control_shader;
    inline constexpr pipeline_stage pipeline_stage_tessellation_evaluation_shader = pipeline::stage::tessellation_evaluation_shader;
    inline constexpr pipeline_stage pipeline_stage_geometry_shader = pipeline::stage::geometry_shader;
    inline constexpr pipeline_stage pipeline_stage_fragment_shader = pipeline::stage::fragment_shader;
    inline constexpr pipeline_stage pipeline_stage_early_fragment_tests = pipeline::stage::early_fragment_tests;
    inline constexpr pipeline_stage pipeline_stage_late_fragment_tests = pipeline::stage::late_fragment_tests;
    inline constexpr pipeline_stage pipeline_stage_color_attachment_output = pipeline::stage::color_attachment_output;
    inline constexpr pipeline_stage pipeline_stage_compute_shader = pipeline::stage::compute_shader;
    inline constexpr pipeline_stage pipeline_stage_transfer = pipeline::stage::transfer;
    inline constexpr pipeline_stage pipeline_stage_bottom_of_pipe = pipeline::stage::bottom_of_pipe;
    inline constexpr pipeline_stage pipeline_stage_host = pipeline::stage::host;
    inline constexpr pipeline_stage pipeline_stage_all_graphics = pipeline::stage::all_graphics;
    inline constexpr pipeline_stage pipeline_stage_all_commands = pipeline::stage::all_commands;
    inline constexpr pipeline_stage pipeline_stage_copy = pipeline::stage::copy;
    inline constexpr pipeline_stage pipeline_stage_resolve = pipeline::stage::resolve;
    inline constexpr pipeline_stage pipeline_stage_blit = pipeline::stage::blit;
    inline constexpr pipeline_stage pipeline_stage_clear = pipeline::stage::clear;
    inline constexpr pipeline_stage pipeline_stage_index_input = pipeline::stage::index_input;
    inline constexpr pipeline_stage pipeline_stage_vertex_attribute_input = pipeline::stage::vertex_attribute_input;
    inline constexpr pipeline_stage pipeline_stage_pre_rasterization_shaders = pipeline::stage::pre_rasterization_shaders;
    inline constexpr pipeline_stage pipeline_stage_video_decode = pipeline::stage::video_decode;
    inline constexpr pipeline_stage pipeline_stage_video_encode = pipeline::stage::video_encode;
    inline constexpr pipeline_stage pipeline_stage_transform_feedback = pipeline::stage::transform_feedback;
    inline constexpr pipeline_stage pipeline_stage_conditional_rendering = pipeline::stage::conditional_rendering;
    inline constexpr pipeline_stage pipeline_stage_command_preprocess = pipeline::stage::command_preprocess;
    inline constexpr pipeline_stage pipeline_stage_fragment_shading_rate_attachment = pipeline::stage::fragment_shading_rate_attachment;
    inline constexpr pipeline_stage pipeline_stage_acceleration_structure_build = pipeline::stage::acceleration_structure_build;
    inline constexpr pipeline_stage pipeline_stage_ray_tracing_shader = pipeline::stage::ray_tracing_shader;
    inline constexpr pipeline_stage pipeline_stage_fragment_density_process = pipeline::stage::fragment_density_process;
    inline constexpr pipeline_stage pipeline_stage_task_shader = pipeline::stage::task_shader;
    inline constexpr pipeline_stage pipeline_stage_mesh_shader = pipeline::stage::mesh_shader;
    inline constexpr pipeline_stage pipeline_stage_subpass_shader = pipeline::stage::subpass_shader;
    inline constexpr pipeline_stage pipeline_stage_invocation_mask = pipeline::stage::invocation_mask;
    inline constexpr pipeline_stage pipeline_stage_acceleration_structure_copy = pipeline::stage::acceleration_structure_copy;
    inline constexpr pipeline_stage pipeline_stage_micromap_build = pipeline::stage::micromap_build;
    inline constexpr pipeline_stage pipeline_stage_cluster_culling_shader = pipeline::stage::cluster_culling_shader;
    inline constexpr pipeline_stage pipeline_stage_optical_flow = pipeline::stage::optical_flow;


} // namespace vkz

template <> struct enable_bitmask_operators<vkz::pipeline::stage> {
    static constexpr bool enable = true;
};
