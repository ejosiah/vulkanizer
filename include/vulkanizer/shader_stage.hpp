#pragma once

#include "enum_bit_flags.hpp"

#include <volk.h>

namespace vkz {
    enum class shader_stage : VkShaderStageFlags {
        none = 0,
        vertex = VK_SHADER_STAGE_VERTEX_BIT,
        tessellation_control = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        tessellation_evaluation = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
        fragment = VK_SHADER_STAGE_FRAGMENT_BIT,
        compute = VK_SHADER_STAGE_COMPUTE_BIT,
        task = VK_SHADER_STAGE_TASK_BIT_EXT,
        mesh = VK_SHADER_STAGE_MESH_BIT_EXT,
        raygen = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        any_hit = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        closest_hit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        miss = VK_SHADER_STAGE_MISS_BIT_KHR,
        intersection = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
        callable = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        subpass_shading = VK_SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI,
        cluster_culling = VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI,
        all_graphics = VK_SHADER_STAGE_ALL_GRAPHICS,
        all = VK_SHADER_STAGE_ALL,
    };
} // namespace vkz

template <> struct enable_bitmask_operators<vkz::shader_stage> {
    static constexpr bool enable = true;
};
