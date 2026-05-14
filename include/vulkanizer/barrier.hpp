#pragma once

#include "vkz.hpp"
#include <vector>

namespace vkz::barrier {

    static std::vector<VkImageMemoryBarrier2> image_memory_barriers;
    static std::vector<VkBufferMemoryBarrier2> buffer_memory_barriers;
    static std::vector<VkMemoryBarrier2> memory_barriers;
    static VkDependencyInfo dependency_info;

    void gpu_to_cpu(VkCommandBuffer command_buffer);

    void fragment_read_to_compute_write(VkCommandBuffer command_buffer);

    void fragment_write_to_fragment_read(VkCommandBuffer command_buffer);

    void fragment_read_to_fragment_write(VkCommandBuffer command_buffer);

    void compute_write_to_fragment_read(VkCommandBuffer command_buffer);

    void compute_write_to_read(VkCommandBuffer command_buffer);

    void compute_write_to_host_read(VkCommandBuffer command_buffer);

    void compute_write_to_transfer_read(VkCommandBuffer command_buffer);

    void compute_write_to_draw_indirect(VkCommandBuffer command_buffer);

    void transfer_write_to_compute_read(VkCommandBuffer command_buffer);

    void transfer_write_to_compute_write(VkCommandBuffer command_buffer);

    void transfer_write_to_fragment_read(VkCommandBuffer command_buffer);

    void acceleration_structure_update_to_ray_trace_read(VkCommandBuffer command_buffer);

    void acceleration_structure_update_to_ray_query_read(VkCommandBuffer command_buffer);

    void ray_trace_read_to_acceleration_structure_update(VkCommandBuffer command_buffer);

    void ray_query_read_to_acceleration_structure_update(VkCommandBuffer command_buffer);

    void ray_trace_write_to_compute_read(VkCommandBuffer command_buffer);

    void ray_trace_write_to_fragment_read(VkCommandBuffer command_buffer);


    void push(VkImage& image, VkImageSubresourceRange subresource_range,
                    VkPipelineStageFlags2 src_stage_mask,VkPipelineStageFlags2 dst_stage_mask,
                    VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask,
                    VkImageLayout old_layout, VkImageLayout new_layout);

    void push_and_flush(VkCommandBuffer command_buffer, VkImage& image, VkImageSubresourceRange subresource_range,
                             VkPipelineStageFlags2 src_stage_mask,VkPipelineStageFlags2 dst_stage_mask,
                             VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask,
                             VkImageLayout old_layout, VkImageLayout new_layout);

    void push(VkPipelineStageFlags2 src_stage_mask,VkPipelineStageFlags2 dst_stage_mask,
                        VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask);

    void push_and_flush(VkCommandBuffer command_buffer, VkPipelineStageFlags2 src_stage_mask,VkPipelineStageFlags2 dst_stage_mask,
                        VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask);

    void release(VkImage& image, VkImageSubresourceRange subresource_range,
                        VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
                        VkImageLayout old_layout, VkImageLayout new_layout,
                        uint32_t src_queue_family_index, uint32_t dst_queue_family_index);

    void acquire(VkImage& image, VkImageSubresourceRange subresource_range,
                        VkImageLayout old_layout, VkImageLayout new_layout,
                        uint32_t src_queue_family_index, uint32_t dst_queue_family_index);

    void flush(VkCommandBuffer command_buffer, VkDependencyFlags dependency_flag = 0);

    bool flushed();
}
