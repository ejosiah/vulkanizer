#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cassert>
#include <cinttypes>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "vkz.hpp"

namespace vkz {
    class buffer_builder;
    class image_builder;
    class image_view_builder;
    class sampler_builder;
    struct context;
    struct vma_memory_allocator;
    struct mapping;
    class staging_buffer;

    struct buffer {
        VkBuffer _{};
        VkBufferCreateInfo create_info{};
        VmaAllocation allocation{};
        VmaAllocator allocator{};

        [[nodiscard]] mapping map() const;

        static buffer_builder builder(vma_memory_allocator& allocator);

        static buffer build(vma_memory_allocator& allocator);

        void destroy();

        operator VkBuffer() const {
            return _;
        }
    };

    struct image {
        VkImage handle{};
        VkImageCreateInfo create_info{};
        VmaAllocation allocation{};
        VmaAllocator allocator{};
        VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};

        static image_builder builder(vma_memory_allocator& allocator);

        static image build(vma_memory_allocator& allocator);

        void destroy();

        operator VkImage() const {
            return handle;
        }
    };

    struct image_view {
        VkImageView handle{};
        VkImageViewCreateInfo create_info{};
        VkDevice device{};

        static image_view_builder builder(vkz::device device);

        static image_view build(vkz::device device, const vkz::image& image,
                                VkFormat format = VK_FORMAT_UNDEFINED);

        void destroy();

        operator VkImageView() const {
            return handle;
        }
    };

    struct sampler {
        VkSampler handle{};
        VkSamplerCreateInfo create_info{};
        VkDevice device{};

        static sampler_builder builder(vkz::device device);

        static sampler build(vkz::device device);

        void destroy();

        operator VkSampler() const {
            return handle;
        }
    };

    struct texture {
        image image;
        image_view image_view;
        sampler sampler;

        void destroy();
    };

    struct mapping {
        friend struct buffer;
        friend class staging_buffer;

        void* _{};

        template<typename T>
        T* as() {
            return reinterpret_cast<T*>(_);
        }

        void unmap() {
            if(_) {
                assert(allocation != nullptr && allocator != nullptr);
                vmaUnmapMemory(allocator, allocation);
                _ = nullptr;
            }
        }

        operator void*() const {
            return _;
        }

    private:
        VmaAllocation allocation{};
        VmaAllocator allocator{};

    };

    struct vma_memory_allocator {
        VkInstance instance{VK_NULL_HANDLE};
        VkPhysicalDevice physical_device{VK_NULL_HANDLE};
        VkDevice device{VK_NULL_HANDLE};
        VmaAllocator allocator{VK_NULL_HANDLE};

        static vma_memory_allocator create(const context& context);

        static vma_memory_allocator create_not_owned(VmaAllocator allocator);

        void destroy();

        buffer allocate(VkBufferCreateInfo create_info, VmaMemoryUsage usage);

        image allocate(VkImageCreateInfo create_info, VmaMemoryUsage usage = VMA_MEMORY_USAGE_GPU_ONLY);

        buffer_builder make_buffer_builder();

        image_builder make_image_builder();

        void deallocate(buffer buffer);

        void deallocate(image image);

    private:
        bool owns_allocator {};
    };

    class staging_buffer {
    public:
        class borrowed_memory {
            friend class staging_buffer;

        public:
            borrowed_memory() = default;
            borrowed_memory(const borrowed_memory&) = delete;
            borrowed_memory& operator=(const borrowed_memory&) = delete;
            borrowed_memory(borrowed_memory&& other) noexcept;
            borrowed_memory& operator=(borrowed_memory&& other) = delete;

            [[nodiscard]] void* data() const;

            template<typename T>
            [[nodiscard]] T* as() const {
                return static_cast<T*>(data());
            }

            [[nodiscard]] VkDeviceSize offset() const;
            [[nodiscard]] VkDeviceSize size() const;
            [[nodiscard]] VkBuffer source_buffer() const;
            [[nodiscard]] bool valid() const;

            void copy_from(const void* source, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize destination_offset = 0) const;
            void upload(const void* source, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize destination_offset = 0) const;

            void copy_to(
                VkCommandBuffer command_buffer,
                VkBuffer destination,
                VkDeviceSize destination_offset = 0,
                VkDeviceSize size = VK_WHOLE_SIZE,
                VkDeviceSize source_offset = 0) const;

            void copy_to(
                VkCommandBuffer command_buffer,
                const buffer& destination,
                VkDeviceSize destination_offset = 0,
                VkDeviceSize size = VK_WHOLE_SIZE,
                VkDeviceSize source_offset = 0) const;

        private:
            borrowed_memory(staging_buffer* owner, uint64_t id, VkDeviceSize offset, VkDeviceSize size);

            staging_buffer* owner_{};
            uint64_t id_{};
            VkDeviceSize offset_{};
            VkDeviceSize size_{};
        };

        staging_buffer(vma_memory_allocator& allocator, VkDeviceSize capacity);

        staging_buffer(const staging_buffer&) = delete;
        staging_buffer& operator=(const staging_buffer&) = delete;
        staging_buffer(staging_buffer&&) = delete;
        staging_buffer& operator=(staging_buffer&&) = delete;

        [[nodiscard]] borrowed_memory borrow(VkDeviceSize size, VkDeviceSize alignment = 1);
        void return_memory(borrowed_memory& memory);
        void destroy();

        [[nodiscard]] VkDeviceSize capacity() const;
        [[nodiscard]] VkDeviceSize available() const;
        [[nodiscard]] std::size_t outstanding_borrows() const;
        [[nodiscard]] VkBuffer handle() const;

    private:
        struct range {
            VkDeviceSize offset{};
            VkDeviceSize size{};
        };

        [[nodiscard]] static VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment);
        [[nodiscard]] bool owns(const borrowed_memory& memory) const;
        void validate_subrange(const borrowed_memory& memory, VkDeviceSize offset, VkDeviceSize size) const;
        void merge_free_ranges();

        buffer buffer_{};
        mapping mapping_{};
        VkDeviceSize capacity_{};
        std::vector<range> free_ranges_;
        std::unordered_map<uint64_t, range> borrowed_ranges_;
        uint64_t next_id_{1};
        mutable std::mutex mutex_;
    };

    class buffer_builder {
    public:
        explicit buffer_builder(vma_memory_allocator& allocator);

        buffer_builder& flags(VkBufferCreateFlags value);

        buffer_builder& size(VkDeviceSize value);

        buffer_builder& usage(VkBufferUsageFlags value);

        buffer_builder& sharing_mode(VkSharingMode value);

        buffer_builder& queue_families(const uint32_t* indices, uint32_t count);

        buffer_builder& memory_usage(VmaMemoryUsage value);

        buffer_builder& next(const void* value);

        [[nodiscard]] const VkBufferCreateInfo& create_info() const;

        buffer build() const;

    private:
        vma_memory_allocator* allocator_{};
        VkBufferCreateInfo create_info_{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        VmaMemoryUsage memory_usage_{VMA_MEMORY_USAGE_AUTO};
        void* data_{};
        VkDeviceSize data_size_{};

    };

    class image_builder {
    public:
        explicit image_builder(vma_memory_allocator& allocator);

        image_builder& flags(VkImageCreateFlags value);

        image_builder& type(VkImageType value);

        image_builder& format(VkFormat value);

        image_builder& extent(VkExtent3D value);

        image_builder& extent(uint32_t width, uint32_t height, uint32_t depth = 1);

        image_builder& mip_levels(uint32_t value);

        image_builder& array_layers(uint32_t value);

        image_builder& samples(VkSampleCountFlagBits value);

        image_builder& tiling(VkImageTiling value);

        image_builder& usage(VkImageUsageFlags value);

        image_builder& sharing_mode(VkSharingMode value);

        image_builder& queue_families(const uint32_t* indices, uint32_t count);

        image_builder& initial_layout(VkImageLayout value);

        image_builder& memory_usage(VmaMemoryUsage value);

        image_builder& next(const void* value);

        [[nodiscard]] const VkImageCreateInfo& create_info() const;

        [[nodiscard]] image build() const;

    private:
        vma_memory_allocator* allocator_{};
        VkImageCreateInfo create_info_{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        VmaMemoryUsage memory_usage_{VMA_MEMORY_USAGE_GPU_ONLY};
    };

    class image_view_builder {
    public:
        explicit image_view_builder(vkz::device device);

        image_view_builder& flags(VkImageViewCreateFlags value);

        image_view_builder& image(const vkz::image& value);

        image_view_builder& view_type(VkImageViewType value);

        image_view_builder& format(VkFormat value);

        image_view_builder& components(VkComponentMapping value);

        image_view_builder& subresource_range(VkImageSubresourceRange value);

        image_view_builder& aspect_mask(VkImageAspectFlags value);

        image_view_builder& base_mip_level(uint32_t value);

        image_view_builder& level_count(uint32_t value);

        image_view_builder& base_array_layer(uint32_t value);

        image_view_builder& layer_count(uint32_t value);

        image_view_builder& next(const void* value);

        [[nodiscard]] const VkImageViewCreateInfo& create_info() const;

        [[nodiscard]] image_view build() const;

    private:
        vkz::device device_{};
        VkImageViewCreateInfo create_info_{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    };

    class sampler_builder {
    public:
        explicit sampler_builder(vkz::device device);

        sampler_builder& flags(VkSamplerCreateFlags value);

        sampler_builder& mag_filter(VkFilter value);

        sampler_builder& min_filter(VkFilter value);

        sampler_builder& mipmap_mode(VkSamplerMipmapMode value);

        sampler_builder& address_mode(VkSamplerAddressMode value);

        sampler_builder& address_mode_u(VkSamplerAddressMode value);

        sampler_builder& address_mode_v(VkSamplerAddressMode value);

        sampler_builder& address_mode_w(VkSamplerAddressMode value);

        sampler_builder& mip_lod_bias(float value);

        sampler_builder& anisotropy(bool enabled, float max = 1.0f);

        sampler_builder& compare(bool enabled, VkCompareOp op = VK_COMPARE_OP_ALWAYS);

        sampler_builder& min_lod(float value);

        sampler_builder& max_lod(float value);

        sampler_builder& border_color(VkBorderColor value);

        sampler_builder& unnormalized_coordinates(bool enabled);

        sampler_builder& next(const void* value);

        [[nodiscard]] const VkSamplerCreateInfo& create_info() const;

        [[nodiscard]] sampler build() const;

    private:
        vkz::device device_{};
        VkSamplerCreateInfo create_info_{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    };
}
