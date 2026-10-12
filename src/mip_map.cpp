#include "vulkanizer/mip_map.hpp"
#include "vulkanizer/barrier.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vkz {
    namespace {
        vkz::format_feature format_features(const image& image) {
            VmaAllocatorInfo allocator_info{};
            vmaGetAllocatorInfo(image.allocator, &allocator_info);

            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(
                allocator_info.physicalDevice, image.create_info.format, &properties);
            return static_cast<vkz::format_feature>(image.create_info.tiling == VK_IMAGE_TILING_LINEAR
                ? properties.linearTilingFeatures
                : properties.optimalTilingFeatures);
        }

        void validate(const image& image, VkFilter filter) {
            if (image.handle == VK_NULL_HANDLE) {
                throw std::invalid_argument{"image must be valid"};
            }
            if (image.create_info.extent.width == 0 || image.create_info.extent.height == 0 ||
                image.create_info.extent.depth == 0) {
                throw std::runtime_error{"width/height/depth must be greater than 0"};
            }
            if (image.create_info.mipLevels == 0 || image.create_info.arrayLayers == 0) {
                throw std::invalid_argument{"image must have at least one mip level and array layer"};
            }
            if ((image.create_info.usage & static_cast<VkImageUsageFlags>(vkz::image::usage::transfer_src)) == 0 ||
                (image.create_info.usage & static_cast<VkImageUsageFlags>(vkz::image::usage::transfer_dst)) == 0) {
                throw std::invalid_argument{
                    "image requires VK_IMAGE_USAGE_TRANSFER_SRC_BIT and VK_IMAGE_USAGE_TRANSFER_DST_BIT"};
            }
            if (image.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
                throw std::invalid_argument{"base mip level must contain data in a defined layout"};
            }

            const auto features = format_features(image);
            const auto required_features =
                vkz::format_feature::blit;
            if ((features & required_features) != required_features) {
                throw std::runtime_error{"image format does not support blitting"};
            }
            if (filter == VK_FILTER_LINEAR &&
                (features & vkz::format_feature::sampled_image_filter_linear) == vkz::format_feature::none) {
                throw std::runtime_error{"image format does not support linear blit filtering"};
            }
        }
    }

    void generate_mip_maps(VkCommandBuffer command_buffer, image& image, VkFilter filter) {
        if (command_buffer == VK_NULL_HANDLE) {
            throw std::invalid_argument{"command buffer must be valid"};
        }
        validate(image, filter);

        const auto levels = image.create_info.mipLevels;
        const auto layers = image.create_info.arrayLayers;
        const auto old_layout = image.layout;
        if (levels == 1) {
            return;
        }

        barrier::push_and_flush(command_buffer, image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, levels, 0, layers}, vkz::pipeline::stage::all_commands,
                                vkz::pipeline::stage::transfer, vkz::access::memory_read_write, vkz::access::transfer_write, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        auto width = static_cast<int32_t>(image.create_info.extent.width);
        auto height = static_cast<int32_t>(image.create_info.extent.height);
        auto depth = static_cast<int32_t>(image.create_info.extent.depth);
        const auto b = 2.0;

        for (uint32_t level = 1; level < levels; ++level) {
            barrier::push_and_flush(command_buffer, image.handle, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), level - 1, 1, 0, layers}, vkz::pipeline::stage::transfer,
                                    vkz::pipeline::stage::transfer, vkz::access::transfer_write, vkz::access::transfer_read, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            const auto next_width = static_cast<int32_t>(std::max(std::ceil(width / b), 1.0));
            const auto next_height = static_cast<int32_t>(std::max(std::ceil(height / b), 1.0));
            const auto next_depth = static_cast<int32_t>(std::max(std::ceil(depth / b), 1.0));
            VkImageBlit blit{};
            blit.srcSubresource = {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), level - 1, 0, layers};
            blit.srcOffsets[1] = {width, height, depth};
            blit.dstSubresource = {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), level, 0, layers};
            blit.dstOffsets[1] = {next_width, next_height, next_depth};

            vkCmdBlitImage(
                command_buffer,
                image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, filter);

            barrier::push_and_flush(command_buffer, image.handle, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), level - 1, 1, 0, layers}, vkz::pipeline::stage::transfer,
                                    vkz::pipeline::stage::all_commands, vkz::access::transfer_read, vkz::access::memory_read_write, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, old_layout);

            width = next_width;
            height = next_height;
            depth = next_depth;
        }

        barrier::push_and_flush(command_buffer, image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), levels - 1, 1, 0, layers}, vkz::pipeline::stage::transfer,
                                vkz::pipeline::stage::all_commands, vkz::access::transfer_write, vkz::access::memory_read_write, old_layout);
    }
}
