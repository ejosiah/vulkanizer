#pragma once

#include "../memory.hpp"
#include "../pipeline.hpp"
#include "../status.hpp"
#include "../texture.hpp"
#include "../barrier.hpp"
#include "functions.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <type_traits>

namespace vkz {

    inline void transition_for_transfer(VkCommandBuffer command_buffer, image& image, VkImageAspectFlags aspect_mask, VkImageLayout layout,
                                        VkAccessFlags2 access_mask) {
        if (image.layout == layout) return;
        const auto undefined = image.layout == VK_IMAGE_LAYOUT_UNDEFINED;
        barrier::push_and_flush(command_buffer, image,
                                {aspect_mask, 0, image.create_info.mipLevels, 0, image.create_info.arrayLayers},
                                undefined ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                undefined ? VK_ACCESS_2_NONE : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                                access_mask, layout);
    }

    inline void bind_descriptor_sets(VkCommandBuffer command_buffer, const pipeline& pipeline, std::span<const descriptor_set> descriptor_sets,
                                     uint32_t first_set = 0, std::span<const uint32_t> dynamic_offsets = {}) {
        const auto sets = map_range(descriptor_sets, [](const auto& set) { return set.handle; });
        vkCmdBindDescriptorSets(command_buffer, pipeline.bind_point, pipeline.layout, first_set, static_cast<uint32_t>(sets.size()), sets.data(),
                                static_cast<uint32_t>(dynamic_offsets.size()), dynamic_offsets.data());
    }

    inline void bind_descriptor_sets(VkCommandBuffer command_buffer, const pipeline& pipeline,
                                     std::initializer_list<descriptor_set> descriptor_sets, uint32_t first_set = 0,
                                     std::initializer_list<uint32_t> dynamic_offsets = {}) {
        bind_descriptor_sets(command_buffer, pipeline, std::span<const descriptor_set>{descriptor_sets.begin(), descriptor_sets.size()}, first_set,
                             std::span<const uint32_t>{dynamic_offsets.begin(), dynamic_offsets.size()});
    }

    inline void bind_pipeline(VkCommandBuffer command_buffer, const pipeline& pipeline) {
        vkCmdBindPipeline(command_buffer, pipeline.bind_point, pipeline.handle);
        bind_descriptor_sets(command_buffer, pipeline, pipeline.descriptor_sets);
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

    inline void bind_vertex_buffers(VkCommandBuffer command_buffer, std::span<const buffer> buffers) {
        const auto offsets = map_range(buffers, [](auto _) { return VkDeviceSize{0}; });
        const auto vk_buffers = map_range(buffers, [](auto buffer) { return buffer._; });
        vkCmdBindVertexBuffers(command_buffer, 0, static_cast<uint32_t>(buffers.size()), vk_buffers.data(), offsets.data());
    }

    inline void bind_vertex_buffers(VkCommandBuffer command_buffer, std::initializer_list<buffer> buffers) {
        bind_vertex_buffers(command_buffer, std::span<const buffer>{buffers.begin(), buffers.size()});
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

    inline void dispatch(VkCommandBuffer command_buffer, uint32_t group_count_x, uint32_t group_count_y = 1, uint32_t group_count_z = 1) {
        vkCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);
    }

    inline void dispatch_indirect(VkCommandBuffer command_buffer, const buffer& indirect_buffer, VkDeviceSize offset = 0) {
        vkCmdDispatchIndirect(command_buffer, indirect_buffer._, offset);
    }

    inline void dispatch_base(VkCommandBuffer command_buffer, uint32_t base_group_x, uint32_t base_group_y, uint32_t base_group_z,
                              uint32_t group_count_x, uint32_t group_count_y = 1, uint32_t group_count_z = 1) {
        vkCmdDispatchBase(command_buffer, base_group_x, base_group_y, base_group_z, group_count_x, group_count_y, group_count_z);
    }

#ifdef VK_ENABLE_BETA_EXTENSIONS
    inline void dispatch_graph(VkCommandBuffer command_buffer, VkDeviceAddress scratch, const VkDispatchGraphCountInfoAMDX& count_info) {
        vkCmdDispatchGraphAMDX(command_buffer, scratch, &count_info);
    }

    inline void dispatch_graph_indirect(VkCommandBuffer command_buffer, VkDeviceAddress scratch,
                                        const VkDispatchGraphCountInfoAMDX& count_info) {
        vkCmdDispatchGraphIndirectAMDX(command_buffer, scratch, &count_info);
    }

    inline void dispatch_graph_indirect_count(VkCommandBuffer command_buffer, VkDeviceAddress scratch, VkDeviceAddress count_info) {
        vkCmdDispatchGraphIndirectCountAMDX(command_buffer, scratch, count_info);
    }
#endif

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
                VKZ_ASSERT(false, "unsupported mesh index type");
                return;
            }

            const auto buffer_size = mesh.index_buffer->create_info.size;
            VKZ_ASSERT(buffer_size % index_size == 0, "index buffer size must be a multiple of its index type");
            const auto index_count = buffer_size / index_size;
            VKZ_ASSERT(index_count <= std::numeric_limits<uint32_t>::max(), "index count exceeds Vulkan's limit");

            draw_indexed(command_buffer, static_cast<uint32_t>(index_count), instance_count);
        }
    }

    inline void copy(VkCommandBuffer command_buffer, const buffer& src_buffer, const buffer& dst_buffer, VkDeviceSize src_offset = 0,
                     VkDeviceSize dst_offset = 0) {
        VKZ_ASSERT(command_buffer != VK_NULL_HANDLE, "a valid command buffer is required");
        VKZ_ASSERT(src_buffer._ != VK_NULL_HANDLE, "a valid source buffer is required");
        VKZ_ASSERT(dst_buffer._ != VK_NULL_HANDLE, "a valid destination buffer is required");
        VKZ_ASSERT(src_offset <= src_buffer.create_info.size, "source offset exceeds the source buffer size");
        VKZ_ASSERT(dst_offset <= dst_buffer.create_info.size, "destination offset exceeds the destination buffer size");

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

    inline void copy(VkCommandBuffer command_buffer, const buffer& src_buffer, image& dst_image,
                     VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
        VKZ_ASSERT(command_buffer != VK_NULL_HANDLE, "a valid command buffer is required");
        VKZ_ASSERT(src_buffer._ != VK_NULL_HANDLE, "a valid source buffer is required");
        VKZ_ASSERT(dst_image.handle != VK_NULL_HANDLE, "a valid destination image is required");
        VKZ_ASSERT(src_buffer.create_info.size > 0, "the source buffer must not be empty");
        VKZ_ASSERT(dst_image.create_info.extent.width > 0 && dst_image.create_info.extent.height > 0 &&
                       dst_image.create_info.extent.depth > 0,
                   "the destination image extent must not be empty");
        VKZ_ASSERT(dst_image.create_info.arrayLayers > 0, "the destination image must have an array layer");

        transition_for_transfer(command_buffer, dst_image, static_cast<VkImageAspectFlags>(aspect), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_ACCESS_2_TRANSFER_WRITE_BIT);

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

    inline void copy(VkCommandBuffer command_buffer, const buffer& src_buffer, image& dst_image, const image_view& dst_image_view) {
        const auto& range = dst_image_view.create_info.subresourceRange;
        const auto mip_level = range.baseMipLevel;
        const auto layer_count =
            range.layerCount == VK_REMAINING_ARRAY_LAYERS ? dst_image.create_info.arrayLayers - range.baseArrayLayer : range.layerCount;

        VKZ_ASSERT(command_buffer != VK_NULL_HANDLE, "a valid command buffer is required");
        VKZ_ASSERT(src_buffer._ != VK_NULL_HANDLE, "a valid source buffer is required");
        VKZ_ASSERT(src_buffer.create_info.size > 0, "the source buffer must not be empty");
        VKZ_ASSERT(dst_image.handle != VK_NULL_HANDLE, "a valid destination image is required");
        VKZ_ASSERT(dst_image_view.handle != VK_NULL_HANDLE, "a valid destination image view is required");
        VKZ_ASSERT(dst_image_view.create_info.image == dst_image.handle, "the image view must reference the destination image");
        VKZ_ASSERT(range.aspectMask != 0, "the image view must select an image aspect");
        VKZ_ASSERT(mip_level < dst_image.create_info.mipLevels, "the image view base mip level is out of range");
        VKZ_ASSERT(range.baseArrayLayer < dst_image.create_info.arrayLayers, "the image view base array layer is out of range");
        VKZ_ASSERT(layer_count > 0 && layer_count <= dst_image.create_info.arrayLayers - range.baseArrayLayer,
                   "the image view array layers are out of range");

        if (command_buffer == VK_NULL_HANDLE || src_buffer._ == VK_NULL_HANDLE || src_buffer.create_info.size == 0 ||
            dst_image.handle == VK_NULL_HANDLE || dst_image_view.handle == VK_NULL_HANDLE || dst_image_view.create_info.image != dst_image.handle ||
            range.aspectMask == 0 || mip_level >= dst_image.create_info.mipLevels || range.baseArrayLayer >= dst_image.create_info.arrayLayers ||
            layer_count == 0 || layer_count > dst_image.create_info.arrayLayers - range.baseArrayLayer) {
            return;
        }

        transition_for_transfer(command_buffer, dst_image, range.aspectMask, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT);

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

    inline void copy(VkCommandBuffer command_buffer, const buffer& src_buffer, texture& texture) {
        copy(command_buffer, src_buffer, texture.image, texture.image_view);
    }

    inline void copy(VkCommandBuffer command_buffer, image& src_image, const buffer& dst_buffer,
                     VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
        VKZ_ASSERT(command_buffer != VK_NULL_HANDLE, "a valid command buffer is required");
        VKZ_ASSERT(src_image.handle != VK_NULL_HANDLE, "a valid source image is required");
        VKZ_ASSERT(dst_buffer._ != VK_NULL_HANDLE, "a valid destination buffer is required");

        if (command_buffer == VK_NULL_HANDLE || src_image.handle == VK_NULL_HANDLE || dst_buffer._ == VK_NULL_HANDLE) return;

        transition_for_transfer(command_buffer, src_image, static_cast<VkImageAspectFlags>(aspect), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_ACCESS_2_TRANSFER_READ_BIT);

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

    inline void copy(VkCommandBuffer command_buffer, image& src_image, const image_view& src_image_view, const buffer& dst_buffer) {
        const auto& range = src_image_view.create_info.subresourceRange;
        const auto mip_level = range.baseMipLevel;
        const auto layer_count =
            range.layerCount == VK_REMAINING_ARRAY_LAYERS ? src_image.create_info.arrayLayers - range.baseArrayLayer : range.layerCount;
        VKZ_ASSERT(src_image_view.create_info.image == src_image.handle, "the image view must reference the source image");
        VKZ_ASSERT(mip_level < src_image.create_info.mipLevels, "the image view base mip level is out of range");
        VKZ_ASSERT(range.baseArrayLayer < src_image.create_info.arrayLayers, "the image view base array layer is out of range");
        VKZ_ASSERT(layer_count > 0 && layer_count <= src_image.create_info.arrayLayers - range.baseArrayLayer,
                   "the image view array layers are out of range");

        if (src_image_view.create_info.image != src_image.handle || mip_level >= src_image.create_info.mipLevels ||
            range.baseArrayLayer >= src_image.create_info.arrayLayers || layer_count == 0 ||
            layer_count > src_image.create_info.arrayLayers - range.baseArrayLayer) {
            return;
        }

        transition_for_transfer(command_buffer, src_image, range.aspectMask, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_2_TRANSFER_READ_BIT);

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

    inline void copy(VkCommandBuffer command_buffer, texture& texture, const buffer& dst_buffer) {
        copy(command_buffer, texture.image, texture.image_view, dst_buffer);
    }

    template <typename T> inline void update(VkCommandBuffer command_buffer, const buffer& buffer, const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "buffer update values must be trivially copyable");
        static_assert(sizeof(T) % 4 == 0, "buffer update sizes must be a multiple of four bytes");
        static_assert(sizeof(T) <= 65536, "buffer updates cannot exceed 65536 bytes");
        VKZ_ASSERT(sizeof(T) <= buffer.create_info.size, "the update value exceeds the destination buffer");
        vkCmdUpdateBuffer(command_buffer, buffer._, 0, sizeof(T), &value);
    }

    template <typename T> inline void fill(VkCommandBuffer command_buffer, const buffer& buffer, const T& value) {
        static_assert(std::is_trivially_copyable_v<T> && sizeof(T) == sizeof(uint32_t),
                      "buffer fill values must be four-byte trivially copyable types");
        VKZ_ASSERT(buffer.create_info.size % 4 == 0, "the destination buffer size must be a multiple of four bytes");
        vkCmdFillBuffer(command_buffer, buffer._, 0, VK_WHOLE_SIZE, std::bit_cast<uint32_t>(value));
    }

    inline VkImageSubresourceRange image_subresource_range(const sub_resource& resource) {
        return {resource.aspect_mask, resource.base_mip_level, resource.level_count, resource.base_array_layer, resource.layer_count};
    }

    template <typename ClearColor>
    inline void clear(VkCommandBuffer command_buffer, image& image, const ClearColor& clear_color, const sub_resource& resource) {
        static_assert(std::is_same_v<std::remove_cv_t<ClearColor>, VkClearColorValue> ||
                          std::is_same_v<std::remove_cv_t<ClearColor>, VkClearDepthStencilValue>,
                      "image clear values must be VkClearColorValue or VkClearDepthStencilValue");
        const auto range = image_subresource_range(resource);
        transition_for_transfer(command_buffer, image, resource.aspect_mask, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        if constexpr (std::is_same_v<std::remove_cv_t<ClearColor>, VkClearColorValue>) {
            vkCmdClearColorImage(command_buffer, image.handle, image.layout, &clear_color, 1, &range);
        } else {
            vkCmdClearDepthStencilImage(command_buffer, image.handle, image.layout, &clear_color, 1, &range);
        }
    }

    template <typename ClearColor>
    inline void clear(VkCommandBuffer command_buffer, image& image, std::span<const ClearColor> clear_colors,
                      std::span<const sub_resource> resources) {
        VKZ_ASSERT(clear_colors.size() == resources.size(), "each clear value must have a matching subresource");
        const auto count = std::min(clear_colors.size(), resources.size());
        for (size_t index = 0; index < count; ++index) clear(command_buffer, image, clear_colors[index], resources[index]);
    }

    template <typename ClearColor>
    inline void clear(VkCommandBuffer command_buffer, image& image, std::initializer_list<ClearColor> clear_colors,
                      std::initializer_list<sub_resource> resources) {
        clear(command_buffer, image, std::span<const ClearColor>{clear_colors.begin(), clear_colors.size()},
              std::span<const sub_resource>{resources.begin(), resources.size()});
    }

    inline void set_viewport(VkCommandBuffer command_buffer, const viewport& viewport) {
        const VkViewport vk_viewport{viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth};
        vkCmdSetViewport(command_buffer, 0, 1, &vk_viewport);
    }

    inline void set_viewports(VkCommandBuffer command_buffer, std::span<const viewport> viewports) {
        const auto vk_viewports = map_range(viewports, [](const auto& viewport) {
            return VkViewport{viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth};
        });
        vkCmdSetViewport(command_buffer, 0, static_cast<uint32_t>(vk_viewports.size()), vk_viewports.data());
    }

    inline void set_viewports(VkCommandBuffer command_buffer, std::initializer_list<viewport> viewports) {
        set_viewports(command_buffer, std::span<const viewport>{viewports.begin(), viewports.size()});
    }

    inline void set_scissor(VkCommandBuffer command_buffer, const rect2d& scissor) {
        const VkRect2D vk_scissor{{scissor.origin.x, scissor.origin.y}, {scissor.dimensions.x, scissor.dimensions.y}};
        vkCmdSetScissor(command_buffer, 0, 1, &vk_scissor);
    }

    inline void set_scissors(VkCommandBuffer command_buffer, std::span<const rect2d> scissors) {
        const auto vk_scissors = map_range(scissors, [](const auto& scissor) {
            return VkRect2D{{scissor.origin.x, scissor.origin.y}, {scissor.dimensions.x, scissor.dimensions.y}};
        });
        vkCmdSetScissor(command_buffer, 0, static_cast<uint32_t>(vk_scissors.size()), vk_scissors.data());
    }

    inline void set_scissors(VkCommandBuffer command_buffer, std::initializer_list<rect2d> scissors) {
        set_scissors(command_buffer, std::span<const rect2d>{scissors.begin(), scissors.size()});
    }


} // namespace vkz
