#include "vulkanizer/barrier.hpp"

#include <initializer_list>
#include <vector>

namespace vkz::barrier {

    namespace {
        void synchronize_buffers(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask,
                                 vkz::access src_access_mask, vkz::access dst_access_mask) {
            std::vector<VkBufferMemoryBarrier2> barriers;
            barriers.reserve(buffers.size());

            for (const auto &buffer: buffers) {
                barriers.push_back({
                                           .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                           .srcStageMask = static_cast<VkPipelineStageFlags2>(src_stage_mask),
                                           .srcAccessMask = static_cast<VkAccessFlags2>(src_access_mask),
                                           .dstStageMask = static_cast<VkPipelineStageFlags2>(dst_stage_mask),
                                           .dstAccessMask = static_cast<VkAccessFlags2>(dst_access_mask),
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
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void compute_write_to_host_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_host_read);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_host), 0, 1, &barrier, 0,
                             VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void compute_write_to_transfer_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_transfer_read);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_transfer), 0, 1, &barrier,
                             0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void transfer_write_to_host_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_transfer_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_host_read);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_transfer), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_host), 0, 1, &barrier, 0,
                             VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void transfer_write_to_compute_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_transfer_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_transfer), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), 0, 1, &barrier,
                             0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void transfer_write_to_compute_read_write(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_transfer_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read_write);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_transfer), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), 0, 1, &barrier,
                             0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void transfer_write_to_compute_write(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_transfer_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_transfer), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), 0, 1, &barrier,
                             0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void fragment_read_to_compute_write(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_fragment_shader), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void fragment_write_to_fragment_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_fragment_shader), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_fragment_shader), 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void fragment_read_to_fragment_write(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_fragment_shader), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_fragment_shader), 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void compute_write_to_fragment_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_fragment_shader), 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void gpu_to_cpu(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, VK_NULL_HANDLE, static_cast<VkAccessFlags>(vkz::access_shader_write), static_cast<VkAccessFlags>(vkz::access_host_read)};
        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_host), 0, 1, &barrier, 0,
                             VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void compute_write_to_draw_indirect(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_indirect_command_read);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_compute_shader), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_draw_indirect), 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void gpu_to_cpu(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_host, vkz::access_shader_write, vkz::access_host_read);
    }

    void fragment_read_to_compute_write(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_fragment_shader, vkz::pipeline_stage_compute_shader, vkz::access_shader_read, vkz::access_shader_write);
    }

    void fragment_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_fragment_shader, vkz::pipeline_stage_fragment_shader, vkz::access_shader_write, vkz::access_shader_read);
    }

    void fragment_read_to_fragment_write(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_fragment_shader, vkz::pipeline_stage_fragment_shader, vkz::access_shader_read, vkz::access_shader_write);
    }

    void compute_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_fragment_shader, vkz::access_shader_write, vkz::access_shader_read);
    }

    void compute_write_to_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_compute_shader, vkz::access_shader_write, vkz::access_shader_read);
    }

    void compute_write_to_host_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_host, vkz::access_shader_write, vkz::access_host_read);
    }

    void compute_write_to_transfer_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_transfer, vkz::access_shader_write, vkz::access_transfer_read);
    }

    void compute_write_to_draw_indirect(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_draw_indirect, vkz::access_shader_write, vkz::access_indirect_command_read);
    }

    void transfer_write_to_compute_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_transfer, vkz::pipeline_stage_compute_shader, vkz::access_transfer_write, vkz::access_shader_read);
    }

    void transfer_write_to_compute_write(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_transfer, vkz::pipeline_stage_compute_shader, vkz::access_transfer_write, vkz::access_shader_write);
    }

    void transfer_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_transfer, vkz::pipeline_stage_fragment_shader, vkz::access_transfer_write, vkz::access_shader_read);
    }

    void acceleration_structure_update_to_ray_trace_read(VkCommandBuffer command_buffer,
                                                         std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_acceleration_structure_build, vkz::pipeline_stage_ray_tracing_shader, vkz::access_acceleration_structure_write,
                            vkz::access_acceleration_structure_read);
    }

    void acceleration_structure_update_to_ray_query_read(VkCommandBuffer command_buffer,
                                                         std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_acceleration_structure_build, vkz::pipeline_stage_fragment_shader | vkz::pipeline_stage_compute_shader,
                            vkz::access_acceleration_structure_write, vkz::access_acceleration_structure_read);
    }

    void ray_trace_read_to_acceleration_structure_update(VkCommandBuffer command_buffer,
                                                         std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_ray_tracing_shader, vkz::pipeline_stage_acceleration_structure_build, vkz::access_acceleration_structure_read,
                            vkz::access_acceleration_structure_write);
    }

    void ray_query_read_to_acceleration_structure_update(VkCommandBuffer command_buffer,
                                                         std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_fragment_shader | vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_acceleration_structure_build,
                            vkz::access_acceleration_structure_read, vkz::access_acceleration_structure_write);
    }

    void ray_trace_write_to_compute_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_ray_tracing_shader, vkz::pipeline_stage_compute_shader, vkz::access_shader_write, vkz::access_shader_read);
    }

    void ray_trace_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        synchronize_buffers(command_buffer, buffers, vkz::pipeline_stage_ray_tracing_shader, vkz::pipeline_stage_fragment_shader, vkz::access_shader_write, vkz::access_shader_read);
    }

    void prepare_draw(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write | vkz::access_transfer_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read);

        auto srcStages = vkz::pipeline_stage_compute_shader | vkz::pipeline_stage_transfer;
        auto dstStages = vkz::pipeline_stage_vertex_input | vkz::pipeline_stage_fragment_shader;
        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(srcStages), static_cast<VkPipelineStageFlags>(dstStages), 0, 1, &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void acceleration_structure_update_to_ray_trace_read(VkCommandBuffer command_buffer) {
        static VkMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                        .srcStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_acceleration_structure_build),
                                        .srcAccessMask = static_cast<VkAccessFlags>(vkz::access_acceleration_structure_write),
                                        .dstStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_ray_tracing_shader),
                                        .dstAccessMask = static_cast<VkAccessFlags>(vkz::access_acceleration_structure_read)};

        static VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void ray_trace_read_to_acceleration_structure_update(VkCommandBuffer command_buffer) {
        static VkMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                        .srcStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_ray_tracing_shader),
                                        .srcAccessMask = static_cast<VkAccessFlags>(vkz::access_acceleration_structure_read),
                                        .dstStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_acceleration_structure_build),
                                        .dstAccessMask = static_cast<VkAccessFlags>(vkz::access_acceleration_structure_write)};

        static VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void ray_trace_write_to_compute_read(VkCommandBuffer command_buffer) {
        static VkMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                        .srcStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_ray_tracing_shader),
                                        .srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write),
                                        .dstStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_compute_shader),
                                        .dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read)};

        static VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void ray_trace_write_to_fragment_read(VkCommandBuffer command_buffer) {
        static VkMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                        .srcStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_ray_tracing_shader),
                                        .srcAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_write),
                                        .dstStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_fragment_shader),
                                        .dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read)};

        static VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void acceleration_structure_update_to_ray_query_read(VkCommandBuffer command_buffer) {
        static VkMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                        .srcStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_acceleration_structure_build),
                                        .srcAccessMask = static_cast<VkAccessFlags>(vkz::access_acceleration_structure_write),
                                        .dstStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_fragment_shader | vkz::pipeline_stage_compute_shader),
                                        .dstAccessMask = static_cast<VkAccessFlags>(vkz::access_acceleration_structure_read)};

        static VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void ray_query_read_to_acceleration_structure_update(VkCommandBuffer command_buffer) {
        static VkMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                        .srcStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_fragment_shader | vkz::pipeline_stage_compute_shader),
                                        .srcAccessMask = static_cast<VkAccessFlags>(vkz::access_acceleration_structure_read),
                                        .dstStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_acceleration_structure_build),
                                        .dstAccessMask = static_cast<VkAccessFlags>(vkz::access_acceleration_structure_write)};

        static VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(command_buffer, &info);
    }

    void transfer_write_to_fragment_read(VkCommandBuffer command_buffer) {
        VkMemoryBarrier barrier{};

        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = static_cast<VkAccessFlags>(vkz::access_transfer_write);
        barrier.dstAccessMask = static_cast<VkAccessFlags>(vkz::access_shader_read);

        vkCmdPipelineBarrier(command_buffer, static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_transfer), static_cast<VkPipelineStageFlags>(vkz::pipeline_stage_fragment_shader), 0, 1,
                             &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
    }

    void push(VkImage &image, VkImageSubresourceRange subresource_range, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask, vkz::access src_access_mask,
              vkz::access dst_access_mask, VkImageLayout old_layout, VkImageLayout new_layout) {
        image_memory_barriers.push_back({
                                                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                .srcStageMask = static_cast<VkPipelineStageFlags2>(src_stage_mask),
                                                .srcAccessMask = static_cast<VkAccessFlags2>(src_access_mask),
                                                .dstStageMask = static_cast<VkPipelineStageFlags2>(dst_stage_mask),
                                                .dstAccessMask = static_cast<VkAccessFlags2>(dst_access_mask),
                                                .oldLayout = old_layout,
                                                .newLayout = new_layout,
                                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                .image = image,
                                                .subresourceRange = subresource_range,
                                        });
    }

    void push(image &image, VkImageSubresourceRange subresource_range, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask, vkz::access src_access_mask,
              vkz::access dst_access_mask, VkImageLayout new_layout) {
        push(image.handle, subresource_range, src_stage_mask, dst_stage_mask, src_access_mask, dst_access_mask,
             image.layout, new_layout);
        image.layout = new_layout;
    }

    void push_and_flush(VkCommandBuffer command_buffer, VkImage &image, VkImageSubresourceRange subresource_range, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask,
                        vkz::access src_access_mask, vkz::access dst_access_mask, VkImageLayout old_layout, VkImageLayout new_layout) {

        push(image, subresource_range, src_stage_mask, dst_stage_mask, src_access_mask, dst_access_mask, old_layout,
             new_layout);
        flush(command_buffer);
    }

    void push_and_flush(VkCommandBuffer command_buffer, image &image, VkImageSubresourceRange subresource_range, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask,
                        vkz::access src_access_mask, vkz::access dst_access_mask, VkImageLayout new_layout) {
        push(image, subresource_range, src_stage_mask, dst_stage_mask, src_access_mask, dst_access_mask, new_layout);
        flush(command_buffer);
    }

    void push(vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask, vkz::access src_access_mask, vkz::access dst_access_mask) {

        memory_barriers.push_back({
                                          .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                          .srcStageMask = static_cast<VkPipelineStageFlags2>(src_stage_mask),
                                          .srcAccessMask = static_cast<VkAccessFlags2>(src_access_mask),
                                          .dstStageMask = static_cast<VkPipelineStageFlags2>(dst_stage_mask),
                                          .dstAccessMask = static_cast<VkAccessFlags2>(dst_access_mask),
                                  });
    }

    void push_and_flush(VkCommandBuffer command_buffer, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask, vkz::access src_access_mask, vkz::access dst_access_mask) {

        push(src_stage_mask, dst_stage_mask, src_access_mask, dst_access_mask);
        flush(command_buffer);
    }

    void release(VkImage &image, VkImageSubresourceRange subresource_range, vkz::pipeline_stage src_stage_mask, vkz::access src_access_mask, VkImageLayout old_layout, VkImageLayout new_layout,
                 uint32_t src_queue_family_index, uint32_t dst_queue_family_index) {

        image_memory_barriers.push_back({
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = static_cast<VkPipelineStageFlags2>(src_stage_mask),
            .srcAccessMask = static_cast<VkAccessFlags2>(src_access_mask),
            .dstStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_none),
            .dstAccessMask = static_cast<VkAccessFlags>(vkz::access_none),
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
            .srcStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_none),
            .srcAccessMask = static_cast<VkAccessFlags2>(vkz::access_none),
            .dstStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_none),
            .dstAccessMask = static_cast<VkAccessFlags>(vkz::access_none),
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = src_queue_family_index,
            .dstQueueFamilyIndex = dst_queue_family_index,
            .image = image,
            .subresourceRange = subresource_range,
        });
    }

    void release(const buffer &buffer, vkz::pipeline_stage src_stage_mask, vkz::access src_access_mask, uint32_t src_queue_family_index, uint32_t dst_queue_family_index) {
        buffer_memory_barriers.push_back({.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                          .srcStageMask = static_cast<VkPipelineStageFlags2>(src_stage_mask),
                                          .srcAccessMask = static_cast<VkAccessFlags2>(src_access_mask),
                                          .dstStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_none),
                                          .dstAccessMask = static_cast<VkAccessFlags2>(vkz::access_none),
                                          .srcQueueFamilyIndex = src_queue_family_index,
                                          .dstQueueFamilyIndex = dst_queue_family_index,
                                          .buffer = buffer,
                                          .offset = 0,
                                          .size = VK_WHOLE_SIZE});
    }

    void acquire(const buffer &buffer, vkz::pipeline_stage dst_stage_mask, vkz::access dst_access_mask, uint32_t src_queue_family_index, uint32_t dst_queue_family_index) {
        buffer_memory_barriers.push_back({.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                          .srcStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_none),
                                          .srcAccessMask = static_cast<VkAccessFlags2>(vkz::access_none),
                                          .dstStageMask = static_cast<VkPipelineStageFlags2>(dst_stage_mask),
                                          .dstAccessMask = static_cast<VkAccessFlags2>(dst_access_mask),
                                          .srcQueueFamilyIndex = src_queue_family_index,
                                          .dstQueueFamilyIndex = dst_queue_family_index,
                                          .buffer = buffer,
                                          .offset = 0,
                                          .size = VK_WHOLE_SIZE});
    }

    void flush(VkCommandBuffer command_buffer, vkz::dependency dependency_flag) {
        dependency_info.imageMemoryBarrierCount = VKZ_COUNT(image_memory_barriers);
        dependency_info.pImageMemoryBarriers = image_memory_barriers.data();
        dependency_info.bufferMemoryBarrierCount = VKZ_COUNT(buffer_memory_barriers);
        dependency_info.pBufferMemoryBarriers = buffer_memory_barriers.data();
        dependency_info.memoryBarrierCount = VKZ_COUNT(memory_barriers);
        dependency_info.pMemoryBarriers = memory_barriers.data();

        dependency_info.dependencyFlags = static_cast<VkDependencyFlags>(dependency_flag);
        vkCmdPipelineBarrier2(command_buffer, &dependency_info);

        image_memory_barriers.clear();
        buffer_memory_barriers.clear();
        memory_barriers.clear();
    }

    bool flushed() {
        return image_memory_barriers.empty() && buffer_memory_barriers.empty() && memory_barriers.empty();
    }

}
