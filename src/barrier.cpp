#include "vulkanizer/barrier.hpp"

namespace vkz::barrier {

    namespace {
        void synchronize_buffers(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers,
                                 VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask,
                                 VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask) {
            std::vector<VkBufferMemoryBarrier2> barriers;
            barriers.reserve(buffers.size());

            for (const auto& buffer : buffers) {
                barriers.push_back({
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    .srcStageMask = src_stage_mask,
                    .srcAccessMask = src_access_mask,
                    .dstStageMask = dst_stage_mask,
                    .dstAccessMask = dst_access_mask,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = buffer,
                    .offset = 0,
                    .size = VK_WHOLE_SIZE,
                });
            }

            const VkDependencyInfo info{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
                .pBufferMemoryBarriers = barriers.data(),
            };
            vkCmdPipelineBarrier2(command_buffer, &info);
        }
    }

    void compute_write_to_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void compute_write_to_host_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void compute_write_to_transfer_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void transfer_write_to_compute_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void transfer_write_to_compute_write(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void fragment_read_to_compute_write(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void fragment_write_to_fragment_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void fragment_read_to_fragment_write(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void compute_write_to_fragment_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void gpu_to_cpu(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER, VK_NULL_HANDLE, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void compute_write_to_draw_indirect(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void gpu_to_cpu(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_HOST_READ_BIT);
    }

    void fragment_read_to_compute_write(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void fragment_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    }

    void fragment_read_to_fragment_write(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                            VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void compute_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    }

    void compute_write_to_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    }

    void compute_write_to_host_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_HOST_READ_BIT);
    }

    void compute_write_to_transfer_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    }

    void compute_write_to_draw_indirect(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    }

    void transfer_write_to_compute_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    }

    void transfer_write_to_compute_write(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void transfer_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    }

    void acceleration_structure_update_to_ray_trace_read(VkCommandBuffer command_buffer,
                                                          std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                            VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                            VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR);
    }

    void acceleration_structure_update_to_ray_query_read(VkCommandBuffer command_buffer,
                                                          std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                            VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR);
    }

    void ray_trace_read_to_acceleration_structure_update(VkCommandBuffer command_buffer,
                                                          std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                            VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                            VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);
    }

    void ray_query_read_to_acceleration_structure_update(VkCommandBuffer command_buffer,
                                                          std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                            VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                            VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);
    }

    void ray_trace_write_to_compute_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    }

    void ray_trace_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers,
                            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    }

    void acceleration_structure_update_to_ray_trace_read(VkCommandBuffer command_buffer) {
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
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void ray_trace_read_to_acceleration_structure_update(VkCommandBuffer command_buffer) {
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
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void ray_trace_write_to_compute_read(VkCommandBuffer command_buffer) {
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
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void ray_trace_write_to_fragment_read(VkCommandBuffer command_buffer) {
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
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void acceleration_structure_update_to_ray_query_read(VkCommandBuffer command_buffer) {
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
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void ray_query_read_to_acceleration_structure_update(VkCommandBuffer command_buffer) {
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
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void transfer_write_to_fragment_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void push(VkImage& image, VkImageSubresourceRange subresource_range,
                 VkPipelineStageFlags2 src_stage_mask,VkPipelineStageFlags2 dst_stage_mask,
                 VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask,
                 VkImageLayout old_layout, VkImageLayout new_layout) {
    image_memory_barriers.push_back({
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = src_stage_mask,
          .srcAccessMask = src_access_mask,
          .dstStageMask = dst_stage_mask,
          .dstAccessMask = dst_access_mask,
          .oldLayout = old_layout,
          .newLayout = new_layout,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = image,
          .subresourceRange = subresource_range,
    });
}

void push_and_flush(VkCommandBuffer command_buffer, VkImage &image,
                            VkImageSubresourceRange subresource_range, VkPipelineStageFlags2 src_stage_mask,
                            VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 src_access_mask,
                            VkAccessFlags2 dst_access_mask, VkImageLayout old_layout, VkImageLayout new_layout) {

    push(image, subresource_range, src_stage_mask, dst_stage_mask, src_access_mask, dst_access_mask, old_layout, new_layout);
    flush(command_buffer);
}

void
push(VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 src_access_mask,
               VkAccessFlags2 dst_access_mask) {

    memory_barriers.push_back({
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
         .srcStageMask = src_stage_mask,
         .srcAccessMask = src_access_mask,
         .dstStageMask = dst_stage_mask,
         .dstAccessMask = dst_access_mask,
    });
}


void
push_and_flush(VkCommandBuffer command_buffer, VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 src_access_mask,
               VkAccessFlags2 dst_access_mask) {

    push(src_stage_mask, dst_stage_mask, src_access_mask, dst_access_mask);
    flush(command_buffer);
}


void release(VkImage &image, VkImageSubresourceRange subresource_range,
                       VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask, VkImageLayout old_layout,
                       VkImageLayout new_layout, uint32_t src_queue_family_index, uint32_t dst_queue_family_index) {

    image_memory_barriers.push_back({
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = src_stage_mask,
          .srcAccessMask = src_access_mask,
          .dstStageMask = VK_PIPELINE_STAGE_NONE,
          .dstAccessMask = VK_ACCESS_NONE,
          .oldLayout = old_layout,
          .newLayout = new_layout,
          .srcQueueFamilyIndex = src_queue_family_index,
          .dstQueueFamilyIndex = dst_queue_family_index,
          .image = image,
          .subresourceRange = subresource_range,
    });
}

void acquire(VkImage &image, VkImageSubresourceRange subresource_range, VkImageLayout old_layout,
                       VkImageLayout new_layout, uint32_t src_queue_family_index, uint32_t dst_queue_family_index) {

    image_memory_barriers.push_back({
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_NONE,
          .srcAccessMask = VK_PIPELINE_STAGE_NONE,
          .dstStageMask = VK_PIPELINE_STAGE_NONE,
          .dstAccessMask = VK_ACCESS_NONE,
          .oldLayout = old_layout,
          .newLayout = new_layout,
          .srcQueueFamilyIndex = src_queue_family_index,
          .dstQueueFamilyIndex = dst_queue_family_index,
          .image = image,
          .subresourceRange = subresource_range,
    });
}

void flush(VkCommandBuffer command_buffer, VkDependencyFlags dependency_flag) {
    dependency_info.imageMemoryBarrierCount = VKZ_COUNT(image_memory_barriers);
    dependency_info.pImageMemoryBarriers = image_memory_barriers.data();
    dependency_info.bufferMemoryBarrierCount = VKZ_COUNT(buffer_memory_barriers);
    dependency_info.pBufferMemoryBarriers = buffer_memory_barriers.data();
    dependency_info.memoryBarrierCount = VKZ_COUNT(memory_barriers);
    dependency_info.pMemoryBarriers = memory_barriers.data();

    dependency_info.dependencyFlags = dependency_flag;
    vkCmdPipelineBarrier2(command_buffer, &dependency_info);

    image_memory_barriers.clear();
    buffer_memory_barriers.clear();
    memory_barriers.clear();
}

bool flushed() {
    return image_memory_barriers.empty() && buffer_memory_barriers.empty() && memory_barriers.empty();
}

}
