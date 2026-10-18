#pragma once

#include <volk.h>
#include "flags.hpp"
#include <cstdint>

namespace vkz {
    enum class descriptor_type : int32_t {
        sampler = VK_DESCRIPTOR_TYPE_SAMPLER,
        combined_image_sampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        sampled_image = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        storage_image = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        uniform_texel_buffer = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        storage_texel_buffer = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        uniform_buffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        storage_buffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        uniform_buffer_dynamic = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        storage_buffer_dynamic = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
        input_attachment = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        inline_uniform_block = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
        acceleration_structure = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        acceleration_structure_nv = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV,
        sample_weight_image = VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM,
        block_match_image = VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM,
        mutable_descriptor = VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
    };
    // Flat aliases retain the original enum type and bitmask operators.
    inline constexpr descriptor_type descriptor_type_sampler = descriptor_type::sampler;
    inline constexpr descriptor_type descriptor_type_combined_image_sampler = descriptor_type::combined_image_sampler;
    inline constexpr descriptor_type descriptor_type_sampled_image = descriptor_type::sampled_image;
    inline constexpr descriptor_type descriptor_type_storage_image = descriptor_type::storage_image;
    inline constexpr descriptor_type descriptor_type_uniform_texel_buffer = descriptor_type::uniform_texel_buffer;
    inline constexpr descriptor_type descriptor_type_storage_texel_buffer = descriptor_type::storage_texel_buffer;
    inline constexpr descriptor_type descriptor_type_uniform_buffer = descriptor_type::uniform_buffer;
    inline constexpr descriptor_type descriptor_type_storage_buffer = descriptor_type::storage_buffer;
    inline constexpr descriptor_type descriptor_type_uniform_buffer_dynamic = descriptor_type::uniform_buffer_dynamic;
    inline constexpr descriptor_type descriptor_type_storage_buffer_dynamic = descriptor_type::storage_buffer_dynamic;
    inline constexpr descriptor_type descriptor_type_input_attachment = descriptor_type::input_attachment;
    inline constexpr descriptor_type descriptor_type_inline_uniform_block = descriptor_type::inline_uniform_block;
    inline constexpr descriptor_type descriptor_type_acceleration_structure = descriptor_type::acceleration_structure;
    inline constexpr descriptor_type descriptor_type_acceleration_structure_nv = descriptor_type::acceleration_structure_nv;
    inline constexpr descriptor_type descriptor_type_sample_weight_image = descriptor_type::sample_weight_image;
    inline constexpr descriptor_type descriptor_type_block_match_image = descriptor_type::block_match_image;
    inline constexpr descriptor_type descriptor_type_mutable_descriptor = descriptor_type::mutable_descriptor;

} // namespace vkz
