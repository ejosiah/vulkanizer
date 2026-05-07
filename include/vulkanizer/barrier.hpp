#pragma once

#include "vkz.hpp"
#include <vector>

namespace vkz::barrier {

    static std::vector<VkImageMemoryBarrier2> imageMemoryBarriers;
    static std::vector<VkBufferMemoryBarrier2> bufferMemoryBarriers;
    static std::vector<VkMemoryBarrier2> memoryBarriers;
    static VkDependencyInfo dependencyInfo;

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


    void push(VkImage& image, VkImageSubresourceRange subresourceRange,
                    VkPipelineStageFlags2 srcStageMask,VkPipelineStageFlags2 dstStageMask,
                    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                    VkImageLayout oldLayout, VkImageLayout newLayout);

    void pushAndFlush(VkCommandBuffer commandBuffer, VkImage& image, VkImageSubresourceRange subresourceRange,
                             VkPipelineStageFlags2 srcStageMask,VkPipelineStageFlags2 dstStageMask,
                             VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                             VkImageLayout oldLayout, VkImageLayout newLayout);

    void push(VkPipelineStageFlags2 srcStageMask,VkPipelineStageFlags2 dstStageMask,
                        VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask);

    void pushAndFlush(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 srcStageMask,VkPipelineStageFlags2 dstStageMask,
                        VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask);

    void release(VkImage& image, VkImageSubresourceRange subresourceRange,
                        VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask,
                        VkImageLayout oldLayout, VkImageLayout newLayout,
                        uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex);

    void acquire(VkImage& image, VkImageSubresourceRange subresourceRange,
                        VkImageLayout oldLayout, VkImageLayout newLayout,
                        uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex);

    void flush(VkCommandBuffer commandBuffer, VkDependencyFlags dependencyFlag = 0);

    bool flushed();
}
