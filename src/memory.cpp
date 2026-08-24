#define VMA_IMPLEMENTATION
#include "vulkanizer/memory.hpp"

#include "vulkanizer/context.hpp"
#include "vulkanizer/status.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

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

    image_view_builder image_view::builder(vkz::device device) {
        return image_view_builder{device};
    }

    image_view image_view::build(vkz::device device, const vkz::image& image, VkFormat format) {
        return builder(device).image(image)
                .format(format == VK_FORMAT_UNDEFINED ? image.create_info.format : format)
                .build();
    }

    void image_view::destroy() {
        if (handle && device) {
            vkDestroyImageView(device, handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    sampler_builder sampler::builder(vkz::device device) {
        return sampler_builder{device};
    }

    sampler sampler::build(vkz::device device) {
        return builder(device).build();
    }

    void sampler::destroy() {
        if (handle && device) {
            vkDestroySampler(device, handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
    }

    vma_memory_allocator vma_memory_allocator::create(const context& context) {
        vma_memory_allocator result{};
        result.instance = context.instance;
        result.device = context.device;

        VmaAllocatorCreateInfo create_info{};
        create_info.vulkanApiVersion = context.api_version;
        create_info.instance = result.instance;
        create_info.physicalDevice = result.device;
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
        result.device.physical = allocator_info.physicalDevice;
        result.device.logical = allocator_info.device;
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

    staging_buffer::borrowed_memory::borrowed_memory(
            staging_buffer* owner,
            uint64_t id,
            VkDeviceSize offset,
            VkDeviceSize size)
        : owner_{owner}
        , id_{id}
        , offset_{offset}
        , size_{size} {
    }

    staging_buffer::borrowed_memory::borrowed_memory(borrowed_memory&& other) noexcept
        : owner_{std::exchange(other.owner_, nullptr)}
        , id_{std::exchange(other.id_, 0)}
        , offset_{std::exchange(other.offset_, 0)}
        , size_{std::exchange(other.size_, 0)} {
    }

    void* staging_buffer::borrowed_memory::data() const {
        if (!valid()) {
            VKZ_THROW("Cannot access returned staging memory")
        }
        return static_cast<std::byte*>(owner_->mapping_._) + offset_;
    }

    VkDeviceSize staging_buffer::borrowed_memory::offset() const {
        return offset_;
    }

    VkDeviceSize staging_buffer::borrowed_memory::size() const {
        return size_;
    }

    VkBuffer staging_buffer::borrowed_memory::source_buffer() const {
        if (!valid()) {
            VKZ_THROW("Cannot access returned staging memory")
        }
        return owner_->buffer_;
    }

    bool staging_buffer::borrowed_memory::valid() const {
        return owner_ && id_;
    }

    void staging_buffer::borrowed_memory::copy_from(
            const void* source,
            VkDeviceSize size,
            VkDeviceSize destination_offset) const {
        if (!source) {
            VKZ_THROW("Cannot upload from a null source")
        }
        const auto upload_size = size == VK_WHOLE_SIZE ? size_ - destination_offset : size;
        owner_->validate_subrange(*this, destination_offset, upload_size);
        std::memcpy(static_cast<std::byte*>(data()) + destination_offset, source, static_cast<std::size_t>(upload_size));
        VKZ_CHECK_VULKAN(vmaFlushAllocation(
            owner_->buffer_.allocator,
            owner_->buffer_.allocation,
            offset_ + destination_offset,
            upload_size));
    }

    void staging_buffer::borrowed_memory::upload(
            const void* source,
            VkDeviceSize size,
            VkDeviceSize destination_offset) const {
        copy_from(source, size, destination_offset);
    }

    void staging_buffer::borrowed_memory::copy_to(
            VkCommandBuffer command_buffer,
            VkBuffer destination,
            VkDeviceSize destination_offset,
            VkDeviceSize size,
            VkDeviceSize source_offset) const {
        if (!command_buffer || !destination) {
            VKZ_THROW("A valid command buffer and destination buffer are required for a staging copy")
        }
        const auto copy_size = size == VK_WHOLE_SIZE ? size_ - source_offset : size;
        owner_->validate_subrange(*this, source_offset, copy_size);

        const VkBufferCopy region{
            .srcOffset = offset_ + source_offset,
            .dstOffset = destination_offset,
            .size = copy_size,
        };
        vkCmdCopyBuffer(command_buffer, source_buffer(), destination, 1, &region);
    }

    void staging_buffer::borrowed_memory::copy_to(
            VkCommandBuffer command_buffer,
            const buffer& destination,
            VkDeviceSize destination_offset,
            VkDeviceSize size,
            VkDeviceSize source_offset) const {
        copy_to(command_buffer, destination._, destination_offset, size, source_offset);
    }

    staging_buffer::staging_buffer(vma_memory_allocator& allocator, VkDeviceSize capacity)
        : capacity_{capacity} {
        if (!capacity_) {
            VKZ_THROW("Staging buffer capacity must be greater than zero")
        }

        buffer_ = buffer::builder(allocator)
            .size(capacity_)
            .usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
            .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .build();
        mapping_ = buffer_.map();
        free_ranges_.push_back({0, capacity_});
    }

    staging_buffer::borrowed_memory staging_buffer::borrow(VkDeviceSize size, VkDeviceSize alignment) {
        if (!size) {
            VKZ_THROW("Cannot borrow an empty staging memory range")
        }
        if (!alignment) {
            VKZ_THROW("Staging memory alignment must be greater than zero")
        }

        std::scoped_lock lock{mutex_};
        for (auto iterator = free_ranges_.begin(); iterator != free_ranges_.end(); ++iterator) {
            const auto aligned_offset = align_up(iterator->offset, alignment);
            if (aligned_offset < iterator->offset) {
                continue;
            }
            const auto padding = aligned_offset - iterator->offset;
            if (padding > iterator->size || size > iterator->size - padding) {
                continue;
            }

            const auto original = *iterator;
            iterator = free_ranges_.erase(iterator);
            if (padding) {
                iterator = free_ranges_.insert(iterator, {original.offset, padding});
                ++iterator;
            }

            const auto used_end = aligned_offset + size;
            const auto original_end = original.offset + original.size;
            if (used_end < original_end) {
                free_ranges_.insert(iterator, {used_end, original_end - used_end});
            }

            const auto id = next_id_++;
            borrowed_ranges_.emplace(id, range{aligned_offset, size});
            return {this, id, aligned_offset, size};
        }

        VKZ_THROW("Insufficient contiguous staging buffer capacity")
    }

    void staging_buffer::return_memory(borrowed_memory& memory) {
        std::scoped_lock lock{mutex_};
        if (!owns(memory)) {
            VKZ_THROW("Cannot return staging memory that is invalid, foreign, or already returned")
        }

        const auto iterator = borrowed_ranges_.find(memory.id_);
        free_ranges_.push_back(iterator->second);
        borrowed_ranges_.erase(iterator);
        merge_free_ranges();

        memory.owner_ = nullptr;
        memory.id_ = 0;
        memory.offset_ = 0;
        memory.size_ = 0;
    }

    void staging_buffer::destroy() {
        std::scoped_lock lock{mutex_};
        if (!borrowed_ranges_.empty()) {
            VKZ_THROW("Cannot destroy a staging buffer while memory is borrowed")
        }
        mapping_.unmap();
        buffer_.destroy();
        free_ranges_.clear();
        capacity_ = 0;
    }

    VkDeviceSize staging_buffer::capacity() const {
        return capacity_;
    }

    VkDeviceSize staging_buffer::available() const {
        std::scoped_lock lock{mutex_};
        VkDeviceSize result{};
        for (const auto& range : free_ranges_) {
            result += range.size;
        }
        return result;
    }

    std::size_t staging_buffer::outstanding_borrows() const {
        std::scoped_lock lock{mutex_};
        return borrowed_ranges_.size();
    }

    VkBuffer staging_buffer::handle() const {
        return buffer_;
    }

    VkDeviceSize staging_buffer::align_up(VkDeviceSize value, VkDeviceSize alignment) {
        const auto remainder = value % alignment;
        if (!remainder) {
            return value;
        }
        const auto increment = alignment - remainder;
        if (value > std::numeric_limits<VkDeviceSize>::max() - increment) {
            return std::numeric_limits<VkDeviceSize>::max();
        }
        return value + increment;
    }

    bool staging_buffer::owns(const borrowed_memory& memory) const {
        if (memory.owner_ != this || !memory.id_) {
            return false;
        }
        const auto iterator = borrowed_ranges_.find(memory.id_);
        return iterator != borrowed_ranges_.end()
            && iterator->second.offset == memory.offset_
            && iterator->second.size == memory.size_;
    }

    void staging_buffer::validate_subrange(
            const borrowed_memory& memory,
            VkDeviceSize offset,
            VkDeviceSize size) const {
        std::scoped_lock lock{mutex_};
        if (!owns(memory)) {
            VKZ_THROW("Cannot use staging memory that is invalid, foreign, or already returned")
        }
        if (offset > memory.size_ || size > memory.size_ - offset) {
            VKZ_THROW("Staging memory operation exceeds the borrowed range")
        }
    }

    void staging_buffer::merge_free_ranges() {
        std::ranges::sort(free_ranges_, {}, &range::offset);
        std::vector<range> merged;
        merged.reserve(free_ranges_.size());
        for (const auto& current : free_ranges_) {
            if (merged.empty() || merged.back().offset + merged.back().size < current.offset) {
                merged.push_back(current);
            } else {
                const auto end = std::max(
                    merged.back().offset + merged.back().size,
                    current.offset + current.size);
                merged.back().size = end - merged.back().offset;
            }
        }
        free_ranges_ = std::move(merged);
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

    image_view_builder::image_view_builder(vkz::device device)
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
        assert(device_.logical != VK_NULL_HANDLE);

        image_view result{};
        result.create_info = create_info_;
        result.device = device_.logical;

        VKZ_CHECK_VULKAN(vkCreateImageView(device_.logical, &create_info_, nullptr, &result.handle));
        return result;
    }

    sampler_builder::sampler_builder(vkz::device device)
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
        assert(device_.logical != VK_NULL_HANDLE);

        sampler result{};
        result.create_info = create_info_;
        result.device = device_.logical;

        VKZ_CHECK_VULKAN(vkCreateSampler(device_.logical, &create_info_, nullptr, &result.handle));
        return result;
    }
}
