#pragma once

#include <vulkan/vulkan.h>

namespace vkz::barrier {

    void gpuToCpu(VkCommandBuffer commandBuffer);

    void fragmentReadToComputeWrite(VkCommandBuffer commandBuffer);

    void fragmentWriteToFragmentRead(VkCommandBuffer commandBuffer);

    void fragmentReadToFragmentWrite(VkCommandBuffer commandBuffer);

    void computeWriteToFragmentRead(VkCommandBuffer commandBuffer);

    void computeWriteToRead(VkCommandBuffer commandBuffer);

    void computeWriteToHostRead(VkCommandBuffer commandBuffer);

    void computeWriteToTransferRead(VkCommandBuffer commandBuffer);

    void computeWriteToDrawIndirect(VkCommandBuffer commandBuffer);

    void transferWriteToComputeRead(VkCommandBuffer commandBuffer);

    void transferWriteToComputeWrite(VkCommandBuffer commandBuffer);

    void transferWriteToFragmentRead(VkCommandBuffer commandBuffer);

    void accelerationStructureUpdateToRayTraceRead(VkCommandBuffer commandBuffer);

    void accelerationStructureUpdateToRayQueryRead(VkCommandBuffer commandBuffer);

    void rayTraceReadToAccelerationStructureUpdate(VkCommandBuffer commandBuffer);

    void rayQueryReadToAccelerationStructureUpdate(VkCommandBuffer commandBuffer);

    void rayTraceWriteToComputeRead(VkCommandBuffer commandBuffer);

    void rayTraceWriteToFragmentRead(VkCommandBuffer commandBuffer);
    
}