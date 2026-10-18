#pragma once

#include "enum_bit_flags.hpp"

#include <volk.h>

namespace vkz {
    // A 64-bit mask preserves synchronization2 access bits. Legacy bits share
    // the same values and convert explicitly at Vulkan API boundaries.
    enum class access : VkAccessFlags2 {
        none = VK_ACCESS_2_NONE,
        indirect_command_read = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        index_read = VK_ACCESS_2_INDEX_READ_BIT,
        vertex_attribute_read = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        uniform_read = VK_ACCESS_2_UNIFORM_READ_BIT,
        input_attachment_read = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
        shader_read = VK_ACCESS_2_SHADER_READ_BIT,
        shader_write = VK_ACCESS_2_SHADER_WRITE_BIT,
        color_attachment_read = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        color_attachment_write = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        depth_stencil_attachment_read = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        depth_stencil_attachment_write = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        transfer_read = VK_ACCESS_2_TRANSFER_READ_BIT,
        transfer_write = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        host_read = VK_ACCESS_2_HOST_READ_BIT,
        host_write = VK_ACCESS_2_HOST_WRITE_BIT,
        memory_read = VK_ACCESS_2_MEMORY_READ_BIT,
        memory_write = VK_ACCESS_2_MEMORY_WRITE_BIT,
        shader_sampled_read = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        shader_storage_read = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        shader_storage_write = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        video_decode_read = VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR,
        video_decode_write = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
        video_encode_read = VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR,
        video_encode_write = VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR,
        transform_feedback_write = VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
        transform_feedback_counter_read = VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
        transform_feedback_counter_write = VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
        conditional_rendering_read = VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT,
        command_preprocess_read = VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_NV,
        command_preprocess_write = VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV,
        fragment_shading_rate_attachment_read = VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
        acceleration_structure_read = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
        acceleration_structure_write = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        fragment_density_map_read = VK_ACCESS_2_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
        color_attachment_read_noncoherent = VK_ACCESS_2_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
        descriptor_buffer_read = VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT,
        invocation_mask_read = VK_ACCESS_2_INVOCATION_MASK_READ_BIT_HUAWEI,
        shader_binding_table_read = VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR,
        micromap_read = VK_ACCESS_2_MICROMAP_READ_BIT_EXT,
        micromap_write = VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT,
        optical_flow_read = VK_ACCESS_2_OPTICAL_FLOW_READ_BIT_NV,
        optical_flow_write = VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV,

        // Combined access to the same resource category.
        shader_read_write = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
        color_attachment_read_write = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        depth_stencil_attachment_read_write = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        transfer_read_write = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
        host_read_write = VK_ACCESS_2_HOST_READ_BIT | VK_ACCESS_2_HOST_WRITE_BIT,
        memory_read_write = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
        shader_storage_read_write = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        video_decode_read_write = VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR | VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
        video_encode_read_write = VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR | VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR,
        transform_feedback_counter_read_write = VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT | VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
        command_preprocess_read_write = VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_NV | VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV,
        acceleration_structure_read_write = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        micromap_read_write = VK_ACCESS_2_MICROMAP_READ_BIT_EXT | VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT,
        optical_flow_read_write = VK_ACCESS_2_OPTICAL_FLOW_READ_BIT_NV | VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV,
    };

    // Flat aliases retain the original enum type and bitmask operators.
    inline constexpr access access_none = access::none;
    inline constexpr access access_indirect_command_read = access::indirect_command_read;
    inline constexpr access access_index_read = access::index_read;
    inline constexpr access access_vertex_attribute_read = access::vertex_attribute_read;
    inline constexpr access access_uniform_read = access::uniform_read;
    inline constexpr access access_input_attachment_read = access::input_attachment_read;
    inline constexpr access access_shader_read = access::shader_read;
    inline constexpr access access_shader_write = access::shader_write;
    inline constexpr access access_color_attachment_read = access::color_attachment_read;
    inline constexpr access access_color_attachment_write = access::color_attachment_write;
    inline constexpr access access_depth_stencil_attachment_read = access::depth_stencil_attachment_read;
    inline constexpr access access_depth_stencil_attachment_write = access::depth_stencil_attachment_write;
    inline constexpr access access_transfer_read = access::transfer_read;
    inline constexpr access access_transfer_write = access::transfer_write;
    inline constexpr access access_host_read = access::host_read;
    inline constexpr access access_host_write = access::host_write;
    inline constexpr access access_memory_read = access::memory_read;
    inline constexpr access access_memory_write = access::memory_write;
    inline constexpr access access_shader_sampled_read = access::shader_sampled_read;
    inline constexpr access access_shader_storage_read = access::shader_storage_read;
    inline constexpr access access_shader_storage_write = access::shader_storage_write;
    inline constexpr access access_video_decode_read = access::video_decode_read;
    inline constexpr access access_video_decode_write = access::video_decode_write;
    inline constexpr access access_video_encode_read = access::video_encode_read;
    inline constexpr access access_video_encode_write = access::video_encode_write;
    inline constexpr access access_transform_feedback_write = access::transform_feedback_write;
    inline constexpr access access_transform_feedback_counter_read = access::transform_feedback_counter_read;
    inline constexpr access access_transform_feedback_counter_write = access::transform_feedback_counter_write;
    inline constexpr access access_conditional_rendering_read = access::conditional_rendering_read;
    inline constexpr access access_command_preprocess_read = access::command_preprocess_read;
    inline constexpr access access_command_preprocess_write = access::command_preprocess_write;
    inline constexpr access access_fragment_shading_rate_attachment_read = access::fragment_shading_rate_attachment_read;
    inline constexpr access access_acceleration_structure_read = access::acceleration_structure_read;
    inline constexpr access access_acceleration_structure_write = access::acceleration_structure_write;
    inline constexpr access access_fragment_density_map_read = access::fragment_density_map_read;
    inline constexpr access access_color_attachment_read_noncoherent = access::color_attachment_read_noncoherent;
    inline constexpr access access_descriptor_buffer_read = access::descriptor_buffer_read;
    inline constexpr access access_invocation_mask_read = access::invocation_mask_read;
    inline constexpr access access_shader_binding_table_read = access::shader_binding_table_read;
    inline constexpr access access_micromap_read = access::micromap_read;
    inline constexpr access access_micromap_write = access::micromap_write;
    inline constexpr access access_optical_flow_read = access::optical_flow_read;
    inline constexpr access access_optical_flow_write = access::optical_flow_write;
    inline constexpr access access_shader_read_write = access::shader_read_write;
    inline constexpr access access_color_attachment_read_write = access::color_attachment_read_write;
    inline constexpr access access_depth_stencil_attachment_read_write = access::depth_stencil_attachment_read_write;
    inline constexpr access access_transfer_read_write = access::transfer_read_write;
    inline constexpr access access_host_read_write = access::host_read_write;
    inline constexpr access access_memory_read_write = access::memory_read_write;
    inline constexpr access access_shader_storage_read_write = access::shader_storage_read_write;
    inline constexpr access access_video_decode_read_write = access::video_decode_read_write;
    inline constexpr access access_video_encode_read_write = access::video_encode_read_write;
    inline constexpr access access_transform_feedback_counter_read_write = access::transform_feedback_counter_read_write;
    inline constexpr access access_command_preprocess_read_write = access::command_preprocess_read_write;
    inline constexpr access access_acceleration_structure_read_write = access::acceleration_structure_read_write;
    inline constexpr access access_micromap_read_write = access::micromap_read_write;
    inline constexpr access access_optical_flow_read_write = access::optical_flow_read_write;
} // namespace vkz

template <> struct enable_bitmask_operators<vkz::access> {
    static constexpr bool enable = true;
};
