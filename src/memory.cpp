#define VMA_IMPLEMENTATION
#include "vulkanizer/memory.hpp"

#include "vulkanizer/context.hpp"
#include "vulkanizer/status.hpp"

namespace vkz {
    mapping buffer::map() const {
        mapping mapping{};
        vmaMapMemory(allocator, allocation, &mapping._);
        mapping.allocation = allocation;
        mapping.allocator = allocator;
        return mapping;
    }

    buffer_builder buffer::builder(vma_memory_allocator& allocator) {
        return buffer_builder{allocator};
    }

    buffer buffer::build(vma_memory_allocator& allocator) {
        return builder(allocator).build();
    }

    void buffer::destroy() {
        if (_ && allocator) {
            vmaDestroyBuffer(allocator, _, allocation);
            _ = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
    }

    image_builder image::builder(vma_memory_allocator& allocator) {
        return image_builder{allocator};
    }

    image image::build(vma_memory_allocator& allocator) {
        return builder(allocator).build();
    }

    void image::destroy() {
        if (handle && allocator) {
            vmaDestroyImage(allocator, handle, allocation);
            handle = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
    }

    image_view_builder image_view::builder(VkDevice device) {
        return image_view_builder{device};
    }

    image_view image_view::build(VkDevice device, VkImage image, VkFormat format) {
        return builder(device).image(image).format(format).build();
    }

    image_view image_view::build(VkDevice device, const vkz::image& image) {
        return builder(device).image(image).build();
    }

    void image_view::destroy() {
        if (handle && device) {
            vkDestroyImageView(device, handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    sampler_builder sampler::builder(VkDevice device) {
        return sampler_builder{device};
    }

    sampler sampler::build(VkDevice device) {
        return builder(device).build();
    }

    void sampler::destroy() {
        if (handle && device) {
            vkDestroySampler(device, handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    void texture::destroy() {
        sampler.destroy();
        image_view.destroy();
        image.destroy();
    }

    vma_memory_allocator vma_memory_allocator::create(const context& context) {
        vma_memory_allocator result{};
        result.instance = context.instance;
        result.physical_device = context.device.physical;
        result.device = context.device.logical;

        VmaAllocatorCreateInfo create_info{};
        create_info.vulkanApiVersion = context.api_version;
        create_info.instance = result.instance;
        create_info.physicalDevice = result.physical_device;
        create_info.device = result.device;

        VmaVulkanFunctions vulkan_functions{};
        vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        vmaImportVulkanFunctionsFromVolk(&create_info, &vulkan_functions);
        create_info.pVulkanFunctions = &vulkan_functions;

        VKZ_CHECK_VULKAN(vmaCreateAllocator(&create_info, &result.allocator));
        result.owns_allocator = true;
        return result;
    }

    vma_memory_allocator vma_memory_allocator::create_not_owned(VmaAllocator allocator) {
        if (!allocator) {
            VKZ_THROW("Cannot create a non-owning memory allocator from a null VmaAllocator")
        }

        VmaAllocatorInfo allocator_info{};
        vmaGetAllocatorInfo(allocator, &allocator_info);

        vma_memory_allocator result{};
        result.instance = allocator_info.instance;
        result.physical_device = allocator_info.physicalDevice;
        result.device = allocator_info.device;
        result.allocator = allocator;
        result.owns_allocator = false;
        return result;
    }

    void vma_memory_allocator::destroy() {
        if (owns_allocator && allocator) {
            vmaDestroyAllocator(allocator);
            allocator = VK_NULL_HANDLE;
        }
    }

    buffer vma_memory_allocator::allocate(VkBufferCreateInfo create_info, VmaMemoryUsage usage) {
        VmaAllocationCreateInfo allocation_info{};
        allocation_info.usage = usage;

        buffer result{};
        result.create_info = create_info;
        result.allocator = allocator;

        VKZ_CHECK_VULKAN(vmaCreateBuffer(allocator, &create_info, &allocation_info, &result._, &result.allocation, nullptr));
        return result;
    }

    image vma_memory_allocator::allocate(VkImageCreateInfo create_info, VmaMemoryUsage usage) {
        if (create_info.format == VK_FORMAT_UNDEFINED) {
            VKZ_THROW("Cannot allocate image with VK_FORMAT_UNDEFINED")
        }

        VmaAllocationCreateInfo allocation_info{};
        allocation_info.usage = usage;

        image result{};
        result.create_info = create_info;
        result.allocator = allocator;
        result.layout = create_info.initialLayout;

        VKZ_CHECK_VULKAN(vmaCreateImage(allocator, &create_info, &allocation_info, &result.handle, &result.allocation, nullptr));
        return result;
    }

    buffer_builder vma_memory_allocator::make_buffer_builder() {
        return buffer_builder{*this};
    }

    image_builder vma_memory_allocator::make_image_builder() {
        return image_builder{*this};
    }

    void vma_memory_allocator::deallocate(buffer buffer) {
        if (buffer._) {
            vmaDestroyBuffer(allocator, buffer._, buffer.allocation);
        }
    }

    void vma_memory_allocator::deallocate(image image) {
        if (image.handle) {
            vmaDestroyImage(allocator, image.handle, image.allocation);
        }
    }

    buffer_builder::buffer_builder(vma_memory_allocator& allocator)
            : allocator_{&allocator} {
        create_info_.size = 1;
        create_info_.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        create_info_.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    buffer_builder& buffer_builder::flags(VkBufferCreateFlags value) {
        create_info_.flags = value;
        return *this;
    }

    buffer_builder& buffer_builder::size(VkDeviceSize value) {
        create_info_.size = value;
        return *this;
    }

    buffer_builder& buffer_builder::usage(VkBufferUsageFlags value) {
        create_info_.usage = value;
        return *this;
    }

    buffer_builder& buffer_builder::sharing_mode(VkSharingMode value) {
        create_info_.sharingMode = value;
        return *this;
    }

    buffer_builder& buffer_builder::queue_families(const uint32_t* indices, uint32_t count) {
        create_info_.pQueueFamilyIndices = indices;
        create_info_.queueFamilyIndexCount = count;
        return *this;
    }

    buffer_builder& buffer_builder::memory_usage(VmaMemoryUsage value) {
        memory_usage_ = value;
        return *this;
    }

    buffer_builder& buffer_builder::next(const void* value) {
        create_info_.pNext = value;
        return *this;
    }

    const VkBufferCreateInfo& buffer_builder::create_info() const {
        return create_info_;
    }

    buffer buffer_builder::build() const {
        assert(allocator_ != nullptr);
        return allocator_->allocate(create_info_, memory_usage_);
    }

    image_builder::image_builder(vma_memory_allocator& allocator)
            : allocator_{&allocator} {
        create_info_.imageType = VK_IMAGE_TYPE_2D;
        create_info_.format = VK_FORMAT_R8G8B8A8_UNORM;
        create_info_.extent = {1, 1, 1};
        create_info_.mipLevels = 1;
        create_info_.arrayLayers = 1;
        create_info_.samples = VK_SAMPLE_COUNT_1_BIT;
        create_info_.tiling = VK_IMAGE_TILING_OPTIMAL;
        create_info_.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        create_info_.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info_.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    image_builder& image_builder::flags(VkImageCreateFlags value) {
        create_info_.flags = value;
        return *this;
    }

    image_builder& image_builder::type(VkImageType value) {
        create_info_.imageType = value;
        return *this;
    }

    image_builder& image_builder::format(VkFormat value) {
        create_info_.format = value;
        return *this;
    }

    image_builder& image_builder::extent(VkExtent3D value) {
        create_info_.extent = value;
        return *this;
    }

    image_builder& image_builder::extent(uint32_t width, uint32_t height, uint32_t depth) {
        create_info_.extent = {width, height, depth};
        return *this;
    }

    image_builder& image_builder::mip_levels(uint32_t value) {
        create_info_.mipLevels = value;
        return *this;
    }

    image_builder& image_builder::array_layers(uint32_t value) {
        create_info_.arrayLayers = value;
        return *this;
    }

    image_builder& image_builder::samples(VkSampleCountFlagBits value) {
        create_info_.samples = value;
        return *this;
    }

    image_builder& image_builder::tiling(VkImageTiling value) {
        create_info_.tiling = value;
        return *this;
    }

    image_builder& image_builder::usage(VkImageUsageFlags value) {
        create_info_.usage = value;
        return *this;
    }

    image_builder& image_builder::sharing_mode(VkSharingMode value) {
        create_info_.sharingMode = value;
        return *this;
    }

    image_builder& image_builder::queue_families(const uint32_t* indices, uint32_t count) {
        create_info_.pQueueFamilyIndices = indices;
        create_info_.queueFamilyIndexCount = count;
        return *this;
    }

    image_builder& image_builder::initial_layout(VkImageLayout value) {
        create_info_.initialLayout = value;
        return *this;
    }

    image_builder& image_builder::memory_usage(VmaMemoryUsage value) {
        memory_usage_ = value;
        return *this;
    }

    image_builder& image_builder::next(const void* value) {
        create_info_.pNext = value;
        return *this;
    }

    const VkImageCreateInfo& image_builder::create_info() const {
        return create_info_;
    }

    image image_builder::build() const {
        assert(allocator_ != nullptr);
        return allocator_->allocate(create_info_, memory_usage_);
    }

    image_view_builder::image_view_builder(vkz::device device) : image_view_builder(device.logical) {}

    image_view_builder::image_view_builder(VkDevice device)
            : device_{device} {
        create_info_.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info_.format = VK_FORMAT_R8G8B8A8_UNORM;
        create_info_.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info_.subresourceRange.baseMipLevel = 0;
        create_info_.subresourceRange.levelCount = 1;
        create_info_.subresourceRange.baseArrayLayer = 0;
        create_info_.subresourceRange.layerCount = 1;
    }

    image_view_builder& image_view_builder::flags(VkImageViewCreateFlags value) {
        create_info_.flags = value;
        return *this;
    }

    image_view_builder& image_view_builder::image(VkImage value) {
        create_info_.image = value;
        return *this;
    }

    image_view_builder& image_view_builder::image(const vkz::image& value) {
        create_info_.image = value.handle;
        create_info_.format = value.create_info.format;
        return *this;
    }

    image_view_builder& image_view_builder::view_type(VkImageViewType value) {
        create_info_.viewType = value;
        return *this;
    }

    image_view_builder& image_view_builder::format(VkFormat value) {
        create_info_.format = value;
        return *this;
    }

    image_view_builder& image_view_builder::components(VkComponentMapping value) {
        create_info_.components = value;
        return *this;
    }

    image_view_builder& image_view_builder::subresource_range(VkImageSubresourceRange value) {
        create_info_.subresourceRange = value;
        return *this;
    }

    image_view_builder& image_view_builder::aspect_mask(VkImageAspectFlags value) {
        create_info_.subresourceRange.aspectMask = value;
        return *this;
    }

    image_view_builder& image_view_builder::base_mip_level(uint32_t value) {
        create_info_.subresourceRange.baseMipLevel = value;
        return *this;
    }

    image_view_builder& image_view_builder::level_count(uint32_t value) {
        create_info_.subresourceRange.levelCount = value;
        return *this;
    }

    image_view_builder& image_view_builder::base_array_layer(uint32_t value) {
        create_info_.subresourceRange.baseArrayLayer = value;
        return *this;
    }

    image_view_builder& image_view_builder::layer_count(uint32_t value) {
        create_info_.subresourceRange.layerCount = value;
        return *this;
    }

    image_view_builder& image_view_builder::next(const void* value) {
        create_info_.pNext = value;
        return *this;
    }

    const VkImageViewCreateInfo& image_view_builder::create_info() const {
        return create_info_;
    }

    image_view image_view_builder::build() const {
        assert(device_ != VK_NULL_HANDLE);

        image_view result{};
        result.create_info = create_info_;
        result.device = device_;

        VKZ_CHECK_VULKAN(vkCreateImageView(device_, &create_info_, nullptr, &result.handle));
        return result;
    }

    sampler_builder::sampler_builder(vkz::device device) : sampler_builder(device.logical) {}

    sampler_builder::sampler_builder(VkDevice device)
            : device_{device} {
        create_info_.magFilter = VK_FILTER_LINEAR;
        create_info_.minFilter = VK_FILTER_LINEAR;
        create_info_.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        create_info_.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info_.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info_.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info_.minLod = 0.0f;
        create_info_.maxLod = VK_LOD_CLAMP_NONE;
        create_info_.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    }

    sampler_builder& sampler_builder::flags(VkSamplerCreateFlags value) {
        create_info_.flags = value;
        return *this;
    }

    sampler_builder& sampler_builder::mag_filter(VkFilter value) {
        create_info_.magFilter = value;
        return *this;
    }

    sampler_builder& sampler_builder::min_filter(VkFilter value) {
        create_info_.minFilter = value;
        return *this;
    }

    sampler_builder& sampler_builder::mipmap_mode(VkSamplerMipmapMode value) {
        create_info_.mipmapMode = value;
        return *this;
    }

    sampler_builder& sampler_builder::address_mode(VkSamplerAddressMode value) {
        create_info_.addressModeU = value;
        create_info_.addressModeV = value;
        create_info_.addressModeW = value;
        return *this;
    }

    sampler_builder& sampler_builder::address_mode_u(VkSamplerAddressMode value) {
        create_info_.addressModeU = value;
        return *this;
    }

    sampler_builder& sampler_builder::address_mode_v(VkSamplerAddressMode value) {
        create_info_.addressModeV = value;
        return *this;
    }

    sampler_builder& sampler_builder::address_mode_w(VkSamplerAddressMode value) {
        create_info_.addressModeW = value;
        return *this;
    }

    sampler_builder& sampler_builder::mip_lod_bias(float value) {
        create_info_.mipLodBias = value;
        return *this;
    }

    sampler_builder& sampler_builder::anisotropy(bool enabled, float max) {
        create_info_.anisotropyEnable = enabled ? VK_TRUE : VK_FALSE;
        create_info_.maxAnisotropy = max;
        return *this;
    }

    sampler_builder& sampler_builder::compare(bool enabled, VkCompareOp op) {
        create_info_.compareEnable = enabled ? VK_TRUE : VK_FALSE;
        create_info_.compareOp = op;
        return *this;
    }

    sampler_builder& sampler_builder::min_lod(float value) {
        create_info_.minLod = value;
        return *this;
    }

    sampler_builder& sampler_builder::max_lod(float value) {
        create_info_.maxLod = value;
        return *this;
    }

    sampler_builder& sampler_builder::border_color(VkBorderColor value) {
        create_info_.borderColor = value;
        return *this;
    }

    sampler_builder& sampler_builder::unnormalized_coordinates(bool enabled) {
        create_info_.unnormalizedCoordinates = enabled ? VK_TRUE : VK_FALSE;
        return *this;
    }

    sampler_builder& sampler_builder::next(const void* value) {
        create_info_.pNext = value;
        return *this;
    }

    const VkSamplerCreateInfo& sampler_builder::create_info() const {
        return create_info_;
    }

    sampler sampler_builder::build() const {
        assert(device_ != VK_NULL_HANDLE);

        sampler result{};
        result.create_info = create_info_;
        result.device = device_;

        VKZ_CHECK_VULKAN(vkCreateSampler(device_, &create_info_, nullptr, &result.handle));
        return result;
    }
}
