#pragma once

#include <volk.h>
#include "flags.hpp"
#include <cstdint>

namespace vkz {
    struct descriptor {
        using binding_flags = vkz::descriptor_binding_flags;
        enum class type : int32_t {
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
    };
} // namespace vkz
