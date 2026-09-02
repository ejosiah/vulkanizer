#pragma once

#include "../memory.hpp"
#include "../pipeline.hpp"
#include "../texture.hpp"
#include "functions.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

namespace vkz {

    inline void bind_pipeline(VkCommandBuffer command_buffer, const pipeline& pipeline) {
        vkCmdBindPipeline(command_buffer, pipeline.bind_point, pipeline.handle);

        const auto& sets = map_range(pipeline.descriptor_sets, [](const auto& set) { return set.handle; });
        vkCmdBindDescriptorSets(command_buffer, pipeline.bind_point, pipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0,
                                VK_NULL_HANDLE);
    }

    template <typename PushConstants>
    inline void push_constants(VkCommandBuffer command_buffer, VkPipelineLayout layout, const PushConstants& constants,
                               VkShaderStageFlags shader_stages = VK_SHADER_STAGE_VERTEX_BIT, uint32_t offset = 0) {
        vkCmdPushConstants(command_buffer, layout, shader_stages, offset, sizeof(PushConstants), &constants);
    }

    template <typename PushConstants>
    inline void push_constants(VkCommandBuffer command_buffer, const pipeline& pipeline, const PushConstants& constants,
                               VkShaderStageFlags shader_stages = VK_SHADER_STAGE_VERTEX_BIT, uint32_t offset = 0) {
        push_constants(command_buffer, pipeline.layout, constants, shader_stages, offset);
    }

    inline void bind_vertex_buffer(VkCommandBuffer command_buffer, const buffer& buffer) {
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer._, &offset);
    }

    inline void bind_vertex_buffers(VkCommandBuffer command_buffer, std::span<buffer> buffers) {
        const auto offsets = map_range(buffers, [](auto _) { return VkDeviceSize{0}; });
        const auto vk_buffers = map_range(buffers, [](auto buffer) { return buffer._; });
        vkCmdBindVertexBuffers(command_buffer, 0, static_cast<uint32_t>(buffers.size()), vk_buffers.data(), offsets.data());
    }

    template <typename index_type = int32_t> inline void bind_index_buffer(VkCommandBuffer command_buffer, const buffer& buffer) {
        auto vk_index_type = VK_INDEX_TYPE_UINT32;
        if constexpr (std::is_same_v<index_type, int8_t>) {
            vk_index_type = VK_INDEX_TYPE_UINT8_EXT;
        } else if constexpr (std::is_same_v<index_type, int16_t>) {
            vk_index_type = VK_INDEX_TYPE_UINT16;
        }
        vkCmdBindIndexBuffer(command_buffer, buffer._, 0, vk_index_type);
    }

    inline void bind_mesh(VkCommandBuffer command_buffer, const mesh& mesh) {
        bind_vertex_buffer(command_buffer, mesh.vertex_buffer);
        if (mesh.index_buffer.has_value()) {
            vkCmdBindIndexBuffer(command_buffer, mesh.index_buffer->_, 0, mesh.index_type);
        }
    }

    inline void draw(VkCommandBuffer command_buffer, uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0,
                     uint32_t first_instance = 0) {
        vkCmdDraw(command_buffer, vertex_count, instance_count, first_vertex, first_instance);
    }

    inline void draw_indexed(VkCommandBuffer command_buffer, uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0,
                             int32_t vertex_offset = 0, uint32_t first_instance = 0) {
        vkCmdDrawIndexed(command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    inline void draw_indirect(VkCommandBuffer command_buffer, const buffer& indirect_buffer, uint32_t draw_count = 1, VkDeviceSize offset = 0,
                              uint32_t stride = sizeof(VkDrawIndirectCommand)) {
        vkCmdDrawIndirect(command_buffer, indirect_buffer._, offset, draw_count, stride);
    }

    inline void draw_indexed_indirect(VkCommandBuffer command_buffer, const buffer& indirect_buffer, uint32_t draw_count = 1, VkDeviceSize offset = 0,
                                      uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)) {
        vkCmdDrawIndexedIndirect(command_buffer, indirect_buffer._, offset, draw_count, stride);
    }

    inline void draw_indirect_count(VkCommandBuffer command_buffer, const buffer& indirect_buffer, const buffer& count_buffer,
                                    uint32_t max_draw_count, VkDeviceSize offset = 0, VkDeviceSize count_offset = 0,
                                    uint32_t stride = sizeof(VkDrawIndirectCommand)) {
        vkCmdDrawIndirectCount(command_buffer, indirect_buffer._, offset, count_buffer._, count_offset, max_draw_count, stride);
    }

    inline void draw_indexed_indirect_count(VkCommandBuffer command_buffer, const buffer& indirect_buffer, const buffer& count_buffer,
                                            uint32_t max_draw_count, VkDeviceSize offset = 0, VkDeviceSize count_offset = 0,
                                            uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)) {
        vkCmdDrawIndexedIndirectCount(command_buffer, indirect_buffer._, offset, count_buffer._, count_offset, max_draw_count, stride);
    }

    inline void bind_and_draw(VkCommandBuffer command_buffer, const mesh& mesh, uint instance_count = 1) {
        bind_mesh(command_buffer, mesh);
        if (mesh.index_buffer.has_value()) {
            VkDeviceSize index_size{};
            switch (mesh.index_type) {
            case VK_INDEX_TYPE_UINT8_EXT:
                index_size = sizeof(uint8_t);
                break;
            case VK_INDEX_TYPE_UINT16:
                index_size = sizeof(uint16_t);
                break;
            case VK_INDEX_TYPE_UINT32:
                index_size = sizeof(uint32_t);
                break;
            default:
                assert(false && "unsupported mesh index type");
                return;
            }

            const auto buffer_size = mesh.index_buffer->create_info.size;
            assert(buffer_size % index_size == 0 && "index buffer size must be a multiple of its index type");
            const auto index_count = buffer_size / index_size;
            assert(index_count <= std::numeric_limits<uint32_t>::max() && "index count exceeds Vulkan's limit");

            draw_indexed(command_buffer, static_cast<uint32_t>(index_count), instance_count);
        }
    }

    inline void copy(VkCommandBuffer command_buffer, const buffer& src_buffer, const buffer& dst_buffer, VkDeviceSize src_offset = 0,
                     VkDeviceSize dst_offset = 0) {
        assert(command_buffer != VK_NULL_HANDLE && "a valid command buffer is required");
        assert(src_buffer._ != VK_NULL_HANDLE && "a valid source buffer is required");
        assert(dst_buffer._ != VK_NULL_HANDLE && "a valid destination buffer is required");
        assert(src_offset <= src_buffer.create_info.size && "source offset exceeds the source buffer size");
        assert(dst_offset <= dst_buffer.create_info.size && "destination offset exceeds the destination buffer size");

        if (command_buffer == VK_NULL_HANDLE || src_buffer._ == VK_NULL_HANDLE || dst_buffer._ == VK_NULL_HANDLE ||
            src_offset > src_buffer.create_info.size || dst_offset > dst_buffer.create_info.size) {
            return;
        }

        const auto copy_size = std::min(src_buffer.create_info.size - src_offset, dst_buffer.create_info.size - dst_offset);
        if (copy_size == 0) {
            return;
        }

        const VkBufferCopy region{
            .srcOffset = src_offset,
            .dstOffset = dst_offset,
            .size = copy_size,
        };
        vkCmdCopyBuffer(command_buffer, src_buffer._, dst_buffer._, 1, &region);
    }

    inline void copy(VkCommandBuffer command_buffer, const buffer& src_buffer, const image& dst_image,
                     VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
        assert(command_buffer != VK_NULL_HANDLE && "a valid command buffer is required");
        assert(src_buffer._ != VK_NULL_HANDLE && "a valid source buffer is required");
        assert(dst_image.handle != VK_NULL_HANDLE && "a valid destination image is required");
        assert(src_buffer.create_info.size > 0 && "the source buffer must not be empty");
        assert(dst_image.create_info.extent.width > 0 && dst_image.create_info.extent.height > 0 && dst_image.create_info.extent.depth > 0 &&
               "the destination image extent must not be empty");
        assert(dst_image.create_info.arrayLayers > 0 && "the destination image must have an array layer");
        assert((dst_image.layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || dst_image.layout == VK_IMAGE_LAYOUT_GENERAL) &&
               "the destination image must be in a transfer destination compatible layout");

        if (command_buffer == VK_NULL_HANDLE || src_buffer._ == VK_NULL_HANDLE || dst_image.handle == VK_NULL_HANDLE ||
            src_buffer.create_info.size == 0 || dst_image.create_info.extent.width == 0 || dst_image.create_info.extent.height == 0 ||
            dst_image.create_info.extent.depth == 0 || dst_image.create_info.arrayLayers == 0 ||
            (dst_image.layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && dst_image.layout != VK_IMAGE_LAYOUT_GENERAL)) {
            return;
        }

        const VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = static_cast<VkImageAspectFlags>(aspect),
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = dst_image.create_info.arrayLayers,
                },
            .imageOffset = {0, 0, 0},
            .imageExtent = dst_image.create_info.extent,
        };
        vkCmdCopyBufferToImage(command_buffer, src_buffer._, dst_image.handle, dst_image.layout, 1, &region);
    }

    inline void copy(VkCommandBuffer command_buffer, const buffer& src_buffer, const image& dst_image, const image_view& dst_image_view) {
        const auto& range = dst_image_view.create_info.subresourceRange;
        const auto mip_level = range.baseMipLevel;
        const auto layer_count =
            range.layerCount == VK_REMAINING_ARRAY_LAYERS ? dst_image.create_info.arrayLayers - range.baseArrayLayer : range.layerCount;

        assert(command_buffer != VK_NULL_HANDLE && "a valid command buffer is required");
        assert(src_buffer._ != VK_NULL_HANDLE && "a valid source buffer is required");
        assert(src_buffer.create_info.size > 0 && "the source buffer must not be empty");
        assert(dst_image.handle != VK_NULL_HANDLE && "a valid destination image is required");
        assert(dst_image_view.handle != VK_NULL_HANDLE && "a valid destination image view is required");
        assert(dst_image_view.create_info.image == dst_image.handle && "the image view must reference the destination image");
        assert(range.aspectMask != 0 && "the image view must select an image aspect");
        assert(mip_level < dst_image.create_info.mipLevels && "the image view base mip level is out of range");
        assert(range.baseArrayLayer < dst_image.create_info.arrayLayers && "the image view base array layer is out of range");
        assert(layer_count > 0 && layer_count <= dst_image.create_info.arrayLayers - range.baseArrayLayer &&
               "the image view array layers are out of range");
        assert((dst_image.layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || dst_image.layout == VK_IMAGE_LAYOUT_GENERAL) &&
               "the destination image must be in a transfer destination compatible layout");

        if (command_buffer == VK_NULL_HANDLE || src_buffer._ == VK_NULL_HANDLE || src_buffer.create_info.size == 0 ||
            dst_image.handle == VK_NULL_HANDLE || dst_image_view.handle == VK_NULL_HANDLE || dst_image_view.create_info.image != dst_image.handle ||
            range.aspectMask == 0 || mip_level >= dst_image.create_info.mipLevels || range.baseArrayLayer >= dst_image.create_info.arrayLayers ||
            layer_count == 0 || layer_count > dst_image.create_info.arrayLayers - range.baseArrayLayer ||
            (dst_image.layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && dst_image.layout != VK_IMAGE_LAYOUT_GENERAL)) {
            return;
        }

        const auto mip_extent = [mip_level](uint32_t extent) { return std::max(1u, extent >> mip_level); };
        const VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = range.aspectMask,
                    .mipLevel = mip_level,
                    .baseArrayLayer = range.baseArrayLayer,
                    .layerCount = layer_count,
                },
            .imageOffset = {0, 0, 0},
            .imageExtent =
                {
                    mip_extent(dst_image.create_info.extent.width),
                    mip_extent(dst_image.create_info.extent.height),
                    mip_extent(dst_image.create_info.extent.depth),
                },
        };
        vkCmdCopyBufferToImage(command_buffer, src_buffer._, dst_image.handle, dst_image.layout, 1, &region);
    }

    inline void copy(VkCommandBuffer command_buffer, const buffer& src_buffer, const texture& texture) {
        copy(command_buffer, src_buffer, texture.image, texture.image_view);
    }

    inline void copy(VkCommandBuffer command_buffer, const image& src_image, const buffer& dst_buffer,
                     VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
        assert(command_buffer != VK_NULL_HANDLE && "a valid command buffer is required");
        assert(src_image.handle != VK_NULL_HANDLE && "a valid source image is required");
        assert(dst_buffer._ != VK_NULL_HANDLE && "a valid destination buffer is required");
        assert((src_image.layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || src_image.layout == VK_IMAGE_LAYOUT_GENERAL) &&
               "the source image must be in a transfer source compatible layout");

        if (command_buffer == VK_NULL_HANDLE || src_image.handle == VK_NULL_HANDLE || dst_buffer._ == VK_NULL_HANDLE ||
            (src_image.layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && src_image.layout != VK_IMAGE_LAYOUT_GENERAL)) {
            return;
        }

        const VkBufferImageCopy region{
            .imageSubresource =
                {
                    .aspectMask = static_cast<VkImageAspectFlags>(aspect),
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = src_image.create_info.arrayLayers,
                },
            .imageExtent = src_image.create_info.extent,
        };
        vkCmdCopyImageToBuffer(command_buffer, src_image.handle, src_image.layout, dst_buffer._, 1, &region);
    }

    inline void copy(VkCommandBuffer command_buffer, const image& src_image, const image_view& src_image_view, const buffer& dst_buffer) {
        const auto& range = src_image_view.create_info.subresourceRange;
        const auto mip_level = range.baseMipLevel;
        const auto layer_count =
            range.layerCount == VK_REMAINING_ARRAY_LAYERS ? src_image.create_info.arrayLayers - range.baseArrayLayer : range.layerCount;
        assert(src_image_view.create_info.image == src_image.handle && "the image view must reference the source image");
        assert(mip_level < src_image.create_info.mipLevels && "the image view base mip level is out of range");
        assert(range.baseArrayLayer < src_image.create_info.arrayLayers && "the image view base array layer is out of range");
        assert(layer_count > 0 && layer_count <= src_image.create_info.arrayLayers - range.baseArrayLayer &&
               "the image view array layers are out of range");

        if (src_image_view.create_info.image != src_image.handle || mip_level >= src_image.create_info.mipLevels ||
            range.baseArrayLayer >= src_image.create_info.arrayLayers || layer_count == 0 ||
            layer_count > src_image.create_info.arrayLayers - range.baseArrayLayer) {
            return;
        }

        const auto mip_extent = [mip_level](uint32_t extent) { return std::max(1u, extent >> mip_level); };
        const VkBufferImageCopy region{
            .imageSubresource =
                {
                    .aspectMask = range.aspectMask,
                    .mipLevel = mip_level,
                    .baseArrayLayer = range.baseArrayLayer,
                    .layerCount = layer_count,
                },
            .imageExtent =
                {
                    mip_extent(src_image.create_info.extent.width),
                    mip_extent(src_image.create_info.extent.height),
                    mip_extent(src_image.create_info.extent.depth),
                },
        };
        vkCmdCopyImageToBuffer(command_buffer, src_image.handle, src_image.layout, dst_buffer._, 1, &region);
    }

    inline void copy(VkCommandBuffer command_buffer, const texture& texture, const buffer& dst_buffer) {
        copy(command_buffer, texture.image, texture.image_view, dst_buffer);
    }

} // namespace vkz
