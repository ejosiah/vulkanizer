#include "vulkanizer/texture.hpp"
#include "vulkanizer/barrier.hpp"
#include "vulkanizer/commands.hpp"
#include "vulkanizer/ktx.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>

namespace {
    struct stbi_deleter {
        void operator()(void *pixels) const noexcept {
            stbi_image_free(pixels);
        }
    };

    bool is_ktx(const std::filesystem::path &path) {
        auto extension = path.extension().string();
        std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return extension == ".ktx" || extension == ".ktx2";
    }

}

namespace vkz {

    void texture::destroy() {
        sampler.destroy();
        image_view.destroy();
        image.destroy();
    }

    texture load(vma_memory_allocator &allocator, VkQueue upload_queue, uint32_t upload_queue_family, const std::filesystem::path &path, VkFormat format, VkSamplerAddressMode addressMode,
                 bool flip_uv) {
        if (is_ktx(path)) {
            return texture_from_ktx(path, allocator, upload_queue, upload_queue_family, addressMode);
        }

        if (upload_queue == VK_NULL_HANDLE) {
            throw std::invalid_argument{"texture upload queue must be valid"};
        }

        int width{};
        int height{};
        int channels{};
        stbi_set_flip_vertically_on_load_thread(flip_uv ? 1 : 0);
        const auto file_path = path.string();
        const bool floating_point = format == VK_FORMAT_R32G32B32A32_SFLOAT;
        std::unique_ptr<void, stbi_deleter> pixels{floating_point ? static_cast<void *>(stbi_loadf(file_path.c_str(), &width, &height, &channels, STBI_rgb_alpha))
                                                                  : static_cast<void *>(stbi_load(file_path.c_str(), &width, &height, &channels, STBI_rgb_alpha))};
        if (!pixels) {
            const char *reason = stbi_failure_reason();
            throw std::runtime_error{std::format("failed to load texture image {}{}{}", file_path, reason ? ": " : "", reason ? reason : "")};
        }
        if (width <= 0 || height <= 0) {
            throw std::runtime_error{std::format("texture image {} has invalid dimensions {}x{}", file_path, width, height)};
        }

        const VkDeviceSize bytes_per_pixel = STBI_rgb_alpha * (floating_point ? sizeof(float) : sizeof(stbi_uc));
        const auto pixel_count = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
        if (pixel_count > std::numeric_limits<VkDeviceSize>::max() / bytes_per_pixel) {
            throw std::runtime_error{std::format("texture image {} is too large", file_path)};
        }
        const VkDeviceSize image_size = pixel_count * bytes_per_pixel;

        buffer staging = buffer::builder(allocator)
            .size(image_size)
            .usage(vkz::buffer::usage::transfer_src)
            .memory_usage(VMA_MEMORY_USAGE_CPU_ONLY)
            .build();
        texture result{};

        try {
            auto mapped = staging.map();
            std::memcpy(mapped.as<void>(), pixels.get(), static_cast<size_t>(image_size));
            VKZ_CHECK_VULKAN(vmaFlushAllocation(staging.allocator, staging.allocation, 0, VK_WHOLE_SIZE));
            mapped.unmap();

            const auto image_usage = vkz::image::usage::sampled_transfer;
            result.image = image::builder(allocator)
                    .type(VK_IMAGE_TYPE_2D)
                    .format(format)
                    .extent(static_cast<uint32_t>(width), static_cast<uint32_t>(height))
                    .mip_levels(1)
                    .array_layers(1)
                    .samples(vkz::sample_count::x1)
                    .tiling(VK_IMAGE_TILING_OPTIMAL)
                    .usage(image_usage)
                    .sharing_mode(VK_SHARING_MODE_EXCLUSIVE)
                    .initial_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                .build();

            command_pool commands{allocator.device, upload_queue_family, vkz::command_pool_flags::transient, upload_queue};
            const auto command_buffer = commands.create_command_buffer();
            const VkImageSubresourceRange range{static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1};

            barrier::push_and_flush(command_buffer, result.image, range, vkz::pipeline::stage::top_of_pipe, vkz::pipeline::stage::transfer, vkz::access::none, vkz::access::transfer_write,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            const VkBufferImageCopy copy{
                .imageSubresource = {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 0, 1},
                .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            };
            vkCmdCopyBufferToImage(command_buffer, staging, result.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

            barrier::push_and_flush(command_buffer, result.image, range, vkz::pipeline::stage::transfer, vkz::pipeline::stage::all_commands, vkz::access::transfer_write, vkz::access::shader_read,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            commands.submit_and_wait(command_buffer);

            result.image_view = image_view::builder(allocator.device)
                .image(result.image)
                .view_type(VK_IMAGE_VIEW_TYPE_2D)
                .format(format)
                .subresource_range(range)
                .build();
            result.sampler = sampler::builder(allocator.device)
                .mag_filter(VK_FILTER_LINEAR)
                .min_filter(VK_FILTER_LINEAR)
                .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                .address_mode(addressMode)
                .min_lod(0.0f)
                .max_lod(0.0f)
                .border_color(VK_BORDER_COLOR_INT_OPAQUE_BLACK)
                .build();
        } catch (...) {
            staging.destroy();
            result.destroy();
            throw;
        }

        staging.destroy();
        return result;
    }
}
