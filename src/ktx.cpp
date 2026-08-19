#include "vulkanizer/ktx.hpp"

#include "vulkanizer/commands.hpp"
#include "vulkanizer/status.hpp"

#include <ktxvulkan.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
    struct ktx_texture_deleter {
        void operator()(ktxTexture* texture) const {
            if (texture) ktxTexture_Destroy(texture);
        }
    };

    using ktx_texture_ptr = std::unique_ptr<ktxTexture, ktx_texture_deleter>;

    void check_ktx(KTX_error_code result, std::string_view operation) {
        if (result != KTX_SUCCESS) {
            throw std::runtime_error{
                std::format("{} failed: {}", operation, ktxErrorString(result))};
        }
    }

    uint32_t mip_dimension(uint32_t base, uint32_t level) {
        return std::max(1u, base >> level);
    }

    VkExtent3D mip_extent(const ktxTexture* texture, uint32_t level) {
        return {
            mip_dimension(texture->baseWidth, level),
            texture->numDimensions > 1 ? mip_dimension(texture->baseHeight, level) : 1u,
            texture->numDimensions > 2 ? mip_dimension(texture->baseDepth, level) : 1u,
        };
    }

    VkImageType image_type(const ktxTexture* texture) {
        switch (texture->numDimensions) {
            case 1: return VK_IMAGE_TYPE_1D;
            case 2: return VK_IMAGE_TYPE_2D;
            case 3: return VK_IMAGE_TYPE_3D;
            default:
                throw std::runtime_error{
                    std::format("unsupported KTX texture dimension: {}", texture->numDimensions)};
        }
    }

    uint32_t array_layer_count(const ktxTexture* texture) {
        const uint32_t layers = std::max(1u, texture->numLayers);
        return texture->isCubemap ? layers * texture->numFaces : layers;
    }

    VkImageViewType image_view_type(const ktxTexture* texture) {
        const uint32_t layers = array_layer_count(texture);
        switch (texture->numDimensions) {
            case 1:
                return layers > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
            case 2:
                if (texture->isCubemap) {
                    return layers > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
                }
                return layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            case 3:
                return VK_IMAGE_VIEW_TYPE_3D;
            default:
                throw std::runtime_error{
                    std::format("unsupported KTX texture dimension: {}", texture->numDimensions)};
        }
    }

    std::vector<VkBufferImageCopy> copy_regions(ktxTexture* texture) {
        if (texture->numDimensions == 3 && texture->numLayers > 1) {
            throw std::runtime_error{"KTX 3D array textures are not supported"};
        }

        const uint32_t levels = std::max(1u, texture->numLevels);
        const uint32_t layers = std::max(1u, texture->numLayers);
        std::vector<VkBufferImageCopy> regions;
        regions.reserve(levels * layers * std::max(1u, std::max(texture->numFaces, texture->baseDepth)));

        for (uint32_t level = 0; level < levels; ++level) {
            const VkExtent3D extent = mip_extent(texture, level);
            const uint32_t face_slices = texture->isCubemap
                ? texture->numFaces
                : texture->numDimensions == 3 ? extent.depth : 1u;

            for (uint32_t layer = 0; layer < layers; ++layer) {
                for (uint32_t face_slice = 0; face_slice < face_slices; ++face_slice) {
                    ktx_size_t offset{};
                    check_ktx(
                        ktxTexture_GetImageOffset(texture, level, layer, face_slice, &offset),
                        std::format("reading KTX image offset for level {}, layer {}, slice {}",
                                    level, layer, face_slice));

                    VkBufferImageCopy region{};
                    region.bufferOffset = offset;
                    region.imageSubresource = {
                        VK_IMAGE_ASPECT_COLOR_BIT,
                        level,
                        texture->numDimensions == 3
                            ? 0u
                            : texture->isCubemap ? layer * texture->numFaces + face_slice : layer,
                        1,
                    };
                    region.imageOffset.z = texture->numDimensions == 3
                        ? static_cast<int32_t>(face_slice)
                        : 0;
                    region.imageExtent = {extent.width, extent.height, 1};
                    regions.push_back(region);
                }
            }
        }
        return regions;
    }

    bool is_integral(VkFormat format) {
        switch (format) {
            case VK_FORMAT_R32_SINT:
            case VK_FORMAT_R32G32_SINT:
            case VK_FORMAT_R32G32B32_SINT:
            case VK_FORMAT_R32G32B32A32_SINT:
            case VK_FORMAT_R32_UINT:
            case VK_FORMAT_R32G32_UINT:
            case VK_FORMAT_R32G32B32_UINT:
            case VK_FORMAT_R32G32B32A32_UINT:
                return true;
            default:
                return false;
        }
    }
}

namespace vkz {
    texture texture_from_ktx(const std::filesystem::path& path,vma_memory_allocator& allocator,VkQueue upload_queue,
                             uint32_t upload_queue_family,VkSamplerAddressMode address_mode,bool enable_anisotropy) {
        const std::string file_path = path.string();
        ktxTexture* raw_texture{};
        check_ktx(
            ktxTexture_CreateFromNamedFile(
                file_path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &raw_texture),
            std::format("loading KTX texture {}", file_path));
        ktx_texture_ptr source{raw_texture};

        if (ktxTexture_NeedsTranscoding(source.get())) {
            if (source->classId != ktxTexture2_c) {
                throw std::runtime_error{
                    std::format("KTX texture {} needs transcoding but is not KTX2", file_path)};
            }
            check_ktx(
                ktxTexture2_TranscodeBasis(
                    reinterpret_cast<ktxTexture2*>(source.get()), KTX_TTF_RGBA32, 0),
                std::format("transcoding KTX texture {}", file_path));
        }

        const VkFormat format = ktxTexture_GetVkFormat(source.get());
        if (format == VK_FORMAT_UNDEFINED) {
            throw std::runtime_error{
                std::format("KTX texture {} has no Vulkan-compatible format", file_path)};
        }

        const VkExtent3D extent = mip_extent(source.get(), 0);
        const uint32_t levels = std::max(1u, source->numLevels);
        const uint32_t layers = array_layer_count(source.get());
        const ktx_size_t data_size = ktxTexture_GetDataSize(source.get());
        const auto* data = ktxTexture_GetData(source.get());
        if (!data || data_size == 0) {
            throw std::runtime_error{std::format("KTX texture {} has no image data", file_path)};
        }

        buffer staging = buffer::builder(allocator)
            .size(data_size)
            .usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
            .memory_usage(VMA_MEMORY_USAGE_CPU_ONLY)
            .build();
        texture result{};

        try {
            auto mapped = staging.map();
            std::memcpy(mapped.as<void>(), data, data_size);
            mapped.unmap();

            result.image = image::builder(allocator)
                .flags(source->isCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0)
                .type(image_type(source.get()))
                .format(format)
                .extent(extent)
                .mip_levels(levels)
                .array_layers(layers)
                .samples(VK_SAMPLE_COUNT_1_BIT)
                .tiling(VK_IMAGE_TILING_OPTIMAL)
                .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                .sharing_mode(VK_SHARING_MODE_EXCLUSIVE)
                .initial_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                .build();

            const VkImageSubresourceRange range{
                VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, layers};
            command_pool commands{
                allocator.device, upload_queue_family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, upload_queue};
            const VkCommandBuffer command_buffer = commands.create_command_buffer();

            VkImageMemoryBarrier to_transfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            to_transfer.srcAccessMask = 0;
            to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_transfer.image = result.image.handle;
            to_transfer.subresourceRange = range;
            vkCmdPipelineBarrier(
                command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &to_transfer);

            const auto regions = copy_regions(source.get());
            vkCmdCopyBufferToImage(
                command_buffer, staging, result.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VKZ_COUNT(regions), regions.data());

            VkImageMemoryBarrier to_shader_read{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            to_shader_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            to_shader_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            to_shader_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader_read.image = result.image.handle;
            to_shader_read.subresourceRange = range;
            vkCmdPipelineBarrier(
                command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &to_shader_read);
            commands.submit_and_wait(command_buffer);
            result.image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            const auto& device = allocator.device;
            result.image_view = image_view::builder(device)
                .image(result.image)
                .view_type(image_view_type(source.get()))
                .format(format)
                .subresource_range(range)
                .build();

            const bool integral = is_integral(format);
            float max_anisotropy = 1.0f;
            if (enable_anisotropy) {
                VkPhysicalDeviceProperties properties{};
                vkGetPhysicalDeviceProperties(device.physical, &properties);
                max_anisotropy = properties.limits.maxSamplerAnisotropy;
            }
            result.sampler = sampler::builder(device)
                .mag_filter(integral ? VK_FILTER_NEAREST : VK_FILTER_LINEAR)
                .min_filter(integral ? VK_FILTER_NEAREST : VK_FILTER_LINEAR)
                .mipmap_mode(integral ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR)
                .address_mode(address_mode)
                .anisotropy(enable_anisotropy, max_anisotropy)
                .min_lod(0.0f)
                .max_lod(static_cast<float>(levels - 1))
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
