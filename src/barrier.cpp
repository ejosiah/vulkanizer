#include "vulkanizer/barrier.hpp"

namespace vkz::barrier {

    void computeWriteToRead(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void computeWriteToHostRead(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void computeWriteToTransferRead(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void transferWriteToComputeRead(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void transferWriteToComputeWrite(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void fragmentReadToComputeWrite(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void fragmentWriteToFragmentRead(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void fragmentReadToFragmentWrite(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void computeWriteToFragmentRead(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void gpuToCpu(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER, VK_NULL_HANDLE, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT };
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void computeWriteToDrawIndirect(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void accelerationStructureUpdateToRayTraceRead(VkCommandBuffer commandBuffer) {
        static VkMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                .dstStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
        };

        static VkDependencyInfo info { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &info);
    }

    void rayTraceReadToAccelerationStructureUpdate(VkCommandBuffer commandBuffer) {
        static VkMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                .dstStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
        };

        static VkDependencyInfo info { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &info);
    }

    void rayTraceWriteToComputeRead(VkCommandBuffer commandBuffer) {
        static VkMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
        };

        static VkDependencyInfo info { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &info);
    }

    void rayTraceWriteToFragmentRead(VkCommandBuffer commandBuffer) {
        static VkMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
        };

        static VkDependencyInfo info { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &info);
    }

    void accelerationStructureUpdateToRayQueryRead(VkCommandBuffer commandBuffer) {
        static VkMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
        };

        static VkDependencyInfo info { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &info);
    }

    void rayQueryReadToAccelerationStructureUpdate(VkCommandBuffer commandBuffer) {
        static VkMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                .dstStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
        };

        static VkDependencyInfo info { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &info);
    }

    void transferWriteToFragmentRead(VkCommandBuffer commandBuffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void push(VkImage& image, VkImageSubresourceRange subresourceRange,
                 VkPipelineStageFlags2 srcStageMask,VkPipelineStageFlags2 dstStageMask,
                 VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                 VkImageLayout oldLayout, VkImageLayout newLayout) {
    imageMemoryBarriers.push_back({
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = srcStageMask,
          .srcAccessMask = srcAccessMask,
          .dstStageMask = dstStageMask,
          .dstAccessMask = dstAccessMask,
          .oldLayout = oldLayout,
          .newLayout = newLayout,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = image,
          .subresourceRange = subresourceRange,
    });
}

void pushAndFlush(VkCommandBuffer commandBuffer, VkImage &image,
                            VkImageSubresourceRange subresourceRange, VkPipelineStageFlags2 srcStageMask,
                            VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 srcAccessMask,
                            VkAccessFlags2 dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout) {

    push(image, subresourceRange, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, oldLayout, newLayout);
    flush(commandBuffer);
}

void
push(VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 srcAccessMask,
               VkAccessFlags2 dstAccessMask) {

    memoryBarriers.push_back({
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
         .srcStageMask = srcStageMask,
         .srcAccessMask = srcAccessMask,
         .dstStageMask = dstStageMask,
         .dstAccessMask = dstAccessMask,
    });
}


void
pushAndFlush(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 srcAccessMask,
               VkAccessFlags2 dstAccessMask) {

    push(srcStageMask, dstStageMask, srcAccessMask, dstAccessMask);
    flush(commandBuffer);
}


void release(VkImage &image, VkImageSubresourceRange subresourceRange,
                       VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkImageLayout oldLayout,
                       VkImageLayout newLayout, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) {

    imageMemoryBarriers.push_back({
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = srcStageMask,
          .srcAccessMask = srcAccessMask,
          .dstStageMask = VK_PIPELINE_STAGE_NONE,
          .dstAccessMask = VK_ACCESS_NONE,
          .oldLayout = oldLayout,
          .newLayout = newLayout,
          .srcQueueFamilyIndex = srcQueueFamilyIndex,
          .dstQueueFamilyIndex = dstQueueFamilyIndex,
          .image = image,
          .subresourceRange = subresourceRange,
    });
}

void acquire(VkImage &image, VkImageSubresourceRange subresourceRange, VkImageLayout oldLayout,
                       VkImageLayout newLayout, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) {

    imageMemoryBarriers.push_back({
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_NONE,
          .srcAccessMask = VK_PIPELINE_STAGE_NONE,
          .dstStageMask = VK_PIPELINE_STAGE_NONE,
          .dstAccessMask = VK_ACCESS_NONE,
          .oldLayout = oldLayout,
          .newLayout = newLayout,
          .srcQueueFamilyIndex = srcQueueFamilyIndex,
          .dstQueueFamilyIndex = dstQueueFamilyIndex,
          .image = image,
          .subresourceRange = subresourceRange,
    });
}

void flush(VkCommandBuffer commandBuffer, VkDependencyFlags dependencyFlag) {
    dependencyInfo.imageMemoryBarrierCount = VKZ_COUNT(imageMemoryBarriers);
    dependencyInfo.pImageMemoryBarriers = imageMemoryBarriers.data();
    dependencyInfo.bufferMemoryBarrierCount = VKZ_COUNT(bufferMemoryBarriers);
    dependencyInfo.pBufferMemoryBarriers = bufferMemoryBarriers.data();
    dependencyInfo.memoryBarrierCount = VKZ_COUNT(memoryBarriers);
    dependencyInfo.pMemoryBarriers = memoryBarriers.data();

    dependencyInfo.dependencyFlags = dependencyFlag;
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

    imageMemoryBarriers.clear();
    bufferMemoryBarriers.clear();
    memoryBarriers.clear();
}

bool flushed() {
    return imageMemoryBarriers.empty() && bufferMemoryBarriers.empty() && memoryBarriers.empty();
}

}