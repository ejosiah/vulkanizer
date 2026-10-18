#pragma once

#include "memory.hpp"
#include "pipeline.hpp"
#include "vkz.hpp"
#include <initializer_list>
#include <vector>

namespace vkz::barrier {

    static std::vector<VkImageMemoryBarrier2> image_memory_barriers;
    static std::vector<VkBufferMemoryBarrier2> buffer_memory_barriers;
    static std::vector<VkMemoryBarrier2> memory_barriers;
    static VkDependencyInfo dependency_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

    void gpu_to_cpu(VkCommandBuffer command_buffer);
    void gpu_to_cpu(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void fragment_read_to_compute_write(VkCommandBuffer command_buffer);
    void fragment_read_to_compute_write(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void fragment_write_to_fragment_read(VkCommandBuffer command_buffer);
    void fragment_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void fragment_read_to_fragment_write(VkCommandBuffer command_buffer);
    void fragment_read_to_fragment_write(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void compute_write_to_fragment_read(VkCommandBuffer command_buffer);
    void compute_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void compute_write_to_read(VkCommandBuffer command_buffer);
    void compute_write_to_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void compute_write_to_host_read(VkCommandBuffer command_buffer);
    void compute_write_to_host_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void compute_write_to_transfer_read(VkCommandBuffer command_buffer);
    void compute_write_to_transfer_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void compute_write_to_draw_indirect(VkCommandBuffer command_buffer);
    void compute_write_to_draw_indirect(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void transfer_write_to_host_read(VkCommandBuffer command_buffer);

    void transfer_write_to_compute_read(VkCommandBuffer command_buffer);
    void transfer_write_to_compute_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);
    void transfer_write_to_compute_read_write(VkCommandBuffer command_buffer);

    void transfer_write_to_compute_write(VkCommandBuffer command_buffer);
    void transfer_write_to_compute_write(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void transfer_write_to_fragment_read(VkCommandBuffer command_buffer);
    void transfer_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void acceleration_structure_update_to_ray_trace_read(VkCommandBuffer command_buffer);
    void acceleration_structure_update_to_ray_trace_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void acceleration_structure_update_to_ray_query_read(VkCommandBuffer command_buffer);
    void acceleration_structure_update_to_ray_query_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void ray_trace_read_to_acceleration_structure_update(VkCommandBuffer command_buffer);
    void ray_trace_read_to_acceleration_structure_update(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void ray_query_read_to_acceleration_structure_update(VkCommandBuffer command_buffer);
    void ray_query_read_to_acceleration_structure_update(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void ray_trace_write_to_compute_read(VkCommandBuffer command_buffer);
    void ray_trace_write_to_compute_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void ray_trace_write_to_fragment_read(VkCommandBuffer command_buffer);
    void ray_trace_write_to_fragment_read(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers);

    void prepare_draw(VkCommandBuffer command_buffer);

    void push(VkImage &image, VkImageSubresourceRange subresource_range, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask, vkz::access src_access_mask,
              vkz::access dst_access_mask, VkImageLayout old_layout, VkImageLayout new_layout);

    void push(image &image, VkImageSubresourceRange subresource_range, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask, vkz::access src_access_mask,
              vkz::access dst_access_mask, VkImageLayout new_layout);

    void push_and_flush(VkCommandBuffer command_buffer, VkImage &image, VkImageSubresourceRange subresource_range, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask,
                        vkz::access src_access_mask, vkz::access dst_access_mask, VkImageLayout old_layout, VkImageLayout new_layout);

    void push_and_flush(VkCommandBuffer command_buffer, image &image, VkImageSubresourceRange subresource_range, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask,
                        vkz::access src_access_mask, vkz::access dst_access_mask, VkImageLayout new_layout);

    void push(vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask, vkz::access src_access_mask, vkz::access dst_access_mask);

    void push_and_flush(VkCommandBuffer command_buffer, vkz::pipeline_stage src_stage_mask, vkz::pipeline_stage dst_stage_mask, vkz::access src_access_mask, vkz::access dst_access_mask);

    void release(VkImage &image, VkImageSubresourceRange subresource_range, vkz::pipeline_stage src_stage_mask, vkz::access src_access_mask, VkImageLayout old_layout, VkImageLayout new_layout,
                 uint32_t src_queue_family_index, uint32_t dst_queue_family_index);

    void acquire(VkImage& image, VkImageSubresourceRange subresource_range,
                        VkImageLayout old_layout, VkImageLayout new_layout,
                        uint32_t src_queue_family_index, uint32_t dst_queue_family_index);

    void release(const buffer &buffer, vkz::pipeline_stage src_stage_mask, vkz::access src_access_mask, uint32_t src_queue_family_index, uint32_t dst_queue_family_index);

    void acquire(const buffer &buffer, vkz::pipeline_stage dst_stage_mask, vkz::access dst_access_mask, uint32_t src_queue_family_index, uint32_t dst_queue_family_index);

    void flush(VkCommandBuffer command_buffer, vkz::dependency dependency_flag = vkz::dependency_none);

    bool flushed();

#ifdef VKZ_ENABLE_VULKAN_COMPATIBILITY
    inline void push(VkImage &image, VkImageSubresourceRange subresource_range, VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask, VkImageLayout old_layout, VkImageLayout new_layout) {
        push(image, subresource_range, static_cast<vkz::pipeline_stage>(src_stage_mask), static_cast<vkz::pipeline_stage>(dst_stage_mask), static_cast<vkz::access>(src_access_mask), static_cast<vkz::access>(dst_access_mask), old_layout, new_layout);
    }
    inline void push(image &image, VkImageSubresourceRange subresource_range, VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask, VkImageLayout new_layout) {
        push(image, subresource_range, static_cast<vkz::pipeline_stage>(src_stage_mask), static_cast<vkz::pipeline_stage>(dst_stage_mask), static_cast<vkz::access>(src_access_mask), static_cast<vkz::access>(dst_access_mask), new_layout);
    }
    inline void push_and_flush(VkCommandBuffer command_buffer, VkImage &image, VkImageSubresourceRange subresource_range, VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask, VkImageLayout old_layout, VkImageLayout new_layout) {
        push_and_flush(command_buffer, image, subresource_range, static_cast<vkz::pipeline_stage>(src_stage_mask), static_cast<vkz::pipeline_stage>(dst_stage_mask), static_cast<vkz::access>(src_access_mask), static_cast<vkz::access>(dst_access_mask), old_layout, new_layout);
    }
    inline void push_and_flush(VkCommandBuffer command_buffer, image &image, VkImageSubresourceRange subresource_range, VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask, VkImageLayout new_layout) {
        push_and_flush(command_buffer, image, subresource_range, static_cast<vkz::pipeline_stage>(src_stage_mask), static_cast<vkz::pipeline_stage>(dst_stage_mask), static_cast<vkz::access>(src_access_mask), static_cast<vkz::access>(dst_access_mask), new_layout);
    }
    inline void push(VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask) {
        push(static_cast<vkz::pipeline_stage>(src_stage_mask), static_cast<vkz::pipeline_stage>(dst_stage_mask), static_cast<vkz::access>(src_access_mask), static_cast<vkz::access>(dst_access_mask));
    }
    inline void push_and_flush(VkCommandBuffer command_buffer, VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask) {
        push_and_flush(command_buffer, static_cast<vkz::pipeline_stage>(src_stage_mask), static_cast<vkz::pipeline_stage>(dst_stage_mask), static_cast<vkz::access>(src_access_mask), static_cast<vkz::access>(dst_access_mask));
    }
    inline void release(VkImage &image, VkImageSubresourceRange subresource_range, VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_queue_family_index, uint32_t dst_queue_family_index) {
        release(image, subresource_range, static_cast<vkz::pipeline_stage>(src_stage_mask), static_cast<vkz::access>(src_access_mask), old_layout, new_layout, src_queue_family_index, dst_queue_family_index);
    }
    inline void release(const buffer &buffer, VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask, uint32_t src_queue_family_index, uint32_t dst_queue_family_index) {
        release(buffer, static_cast<vkz::pipeline_stage>(src_stage_mask), static_cast<vkz::access>(src_access_mask), src_queue_family_index, dst_queue_family_index);
    }
    inline void acquire(const buffer &buffer, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask, uint32_t src_queue_family_index, uint32_t dst_queue_family_index) {
        acquire(buffer, static_cast<vkz::pipeline_stage>(dst_stage_mask), static_cast<vkz::access>(dst_access_mask), src_queue_family_index, dst_queue_family_index);
    }
#endif
} // namespace vkz::barrier
