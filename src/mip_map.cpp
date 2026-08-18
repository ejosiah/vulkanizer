#include "vulkanizer/mip_map.hpp"
#include "vulkanizer/barrier.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vkz {
    namespace {
        VkFormatFeatureFlags format_features(const image& image) {
            VmaAllocatorInfo allocator_info{};
            vmaGetAllocatorInfo(image.allocator, &allocator_info);

            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(
                allocator_info.physicalDevice, image.create_info.format, &properties);
            return image.create_info.tiling == VK_IMAGE_TILING_LINEAR
                ? properties.linearTilingFeatures
                : properties.optimalTilingFeatures;
        }
    }

    void generate_mip_maps(VkCommandBuffer command_buffer, image& image, double k, VkFilter filter) {
        if (command_buffer == VK_NULL_HANDLE || image.handle == VK_NULL_HANDLE) {
            throw std::invalid_argument{"command buffer and image must be valid"};
        }
        if (image.create_info.extent.width == 0 || image.create_info.extent.height == 0 ||
            image.create_info.extent.depth == 0) {
            throw std::runtime_error{"width/height/depth must be greater than 0"};
        }
        if (image.create_info.mipLevels == 0 || image.create_info.arrayLayers == 0) {
            throw std::invalid_argument{"image must have at least one mip level and array layer"};
        }
        if ((image.create_info.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0 ||
            (image.create_info.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
            throw std::invalid_argument{
                "image requires VK_IMAGE_USAGE_TRANSFER_SRC_BIT and VK_IMAGE_USAGE_TRANSFER_DST_BIT"};
        }
        if (image.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
            throw std::invalid_argument{"base mip level must contain data in a defined layout"};
        }
        if (!std::isfinite(k) || k <= 0.0) {
            throw std::invalid_argument{"k must be finite and greater than zero"};
        }

        const auto features = format_features(image);
        const auto required_features = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
        if ((features & required_features) != required_features) {
            throw std::runtime_error{"image format does not support blitting"};
        }
        if (filter == VK_FILTER_LINEAR &&
            (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0) {
            throw std::runtime_error{"image format does not support linear blit filtering"};
        }

        const auto levels = image.create_info.mipLevels;
        const auto layers = image.create_info.arrayLayers;
        const auto old_layout = image.layout;

        barrier::push_and_flush(
            command_buffer, image.handle,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, layers},
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            old_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        auto width = static_cast<int32_t>(image.create_info.extent.width);
        auto height = static_cast<int32_t>(image.create_info.extent.height);
        auto depth = static_cast<int32_t>(image.create_info.extent.depth);

        const auto b = std::pow(2.0, 1.0 / k);
        for (uint32_t level = 1; level < levels; ++level) {
            barrier::push_and_flush(
                command_buffer, image.handle,
                {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, layers},
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            const auto next_width = static_cast<int32_t>(std::max(std::ceil(width / b), 1.0));
            const auto next_height = static_cast<int32_t>(std::max(std::ceil(height / b), 1.0));
            const auto next_depth = static_cast<int32_t>(std::max(std::ceil(depth / b), 1.0));
            VkImageBlit blit{};
            blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, layers};
            blit.srcOffsets[1] = {width, height, depth};
            blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, layers};
            blit.dstOffsets[1] = {next_width, next_height, next_depth};

            vkCmdBlitImage(
                command_buffer,
                image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, filter);

            barrier::push_and_flush(
                command_buffer, image.handle,
                {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, layers},
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, old_layout);

            width = next_width;
            height = next_height;
            depth = next_depth;
        }

        barrier::push_and_flush(
            command_buffer, image.handle,
            {VK_IMAGE_ASPECT_COLOR_BIT, levels - 1, 1, 0, layers},
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, old_layout);
        image.layout = old_layout;
    }
}
