#pragma once

#include <vulkanizer/bvh.hpp>
#include <vulkanizer/barrier.hpp>
#include <algorithm>

namespace vkz::detail {
    inline void bvh_build_barrier(VkCommandBuffer cmd) {
        barrier::push_and_flush(cmd, pipeline_stage_all_commands, pipeline_stage_acceleration_structure_build,
                               access_memory_write | access_acceleration_structure_read,
                               access_shader_read | access_acceleration_structure_read | access_acceleration_structure_write);
    }

    inline void record_bvh_build(VkCommandBuffer cmd, VkAccelerationStructureBuildGeometryInfoKHR info,
                                 uint32_t count, bool update) {
        bvh_build_barrier(cmd);
        info.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        info.srcAccelerationStructure = update ? info.dstAccelerationStructure : VK_NULL_HANDLE;
        VkAccelerationStructureBuildRangeInfoKHR range{count, 0, 0, 0};
        const auto *ranges = &range;
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &info, &ranges);
    }

    inline void finish_bvh_build(VkCommandBuffer cmd) {
        barrier::push_and_flush(cmd, pipeline_stage_acceleration_structure_build, pipeline_stage_all_commands,
                               access_acceleration_structure_write, access_acceleration_structure_read);
    }
}
