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

    // Flat aliases retain the original enum type and bitmask operators.
    inline constexpr shader_stage shader_stage_none = shader_stage::none;
    inline constexpr shader_stage shader_stage_vertex = shader_stage::vertex;
    inline constexpr shader_stage shader_stage_tessellation_control = shader_stage::tessellation_control;
    inline constexpr shader_stage shader_stage_tessellation_evaluation = shader_stage::tessellation_evaluation;
    inline constexpr shader_stage shader_stage_geometry = shader_stage::geometry;
    inline constexpr shader_stage shader_stage_fragment = shader_stage::fragment;
    inline constexpr shader_stage shader_stage_compute = shader_stage::compute;
    inline constexpr shader_stage shader_stage_task = shader_stage::task;
    inline constexpr shader_stage shader_stage_mesh = shader_stage::mesh;
    inline constexpr shader_stage shader_stage_raygen = shader_stage::raygen;
    inline constexpr shader_stage shader_stage_any_hit = shader_stage::any_hit;
    inline constexpr shader_stage shader_stage_closest_hit = shader_stage::closest_hit;
    inline constexpr shader_stage shader_stage_miss = shader_stage::miss;
    inline constexpr shader_stage shader_stage_intersection = shader_stage::intersection;
    inline constexpr shader_stage shader_stage_callable = shader_stage::callable;
    inline constexpr shader_stage shader_stage_subpass_shading = shader_stage::subpass_shading;
    inline constexpr shader_stage shader_stage_cluster_culling = shader_stage::cluster_culling;
    inline constexpr shader_stage shader_stage_all_graphics = shader_stage::all_graphics;
    inline constexpr shader_stage shader_stage_all = shader_stage::all;
} // namespace vkz

template <> struct enable_bitmask_operators<vkz::shader_stage> {
    static constexpr bool enable = true;
};
