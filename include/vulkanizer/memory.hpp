#pragma once

#include "vkz.hpp"
#include "enum_bit_flags.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cassert>
#include <cinttypes>
#include <cstddef>
#include <map>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <optional>

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
        using flags = vkz::buffer_flags;
        enum class usage : VkBufferUsageFlags {
            none = 0,
            transfer_src = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            transfer_dst = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            uniform_texel = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
            storage_texel = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
            uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            indirect = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            device_address = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            conditional_rendering = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT,
            transform_feedback = VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT,
            transform_feedback_counter = VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT,
            video_decode_src = VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
            video_decode_dst = VK_BUFFER_USAGE_VIDEO_DECODE_DST_BIT_KHR,
            video_encode_src = VK_BUFFER_USAGE_VIDEO_ENCODE_SRC_BIT_KHR,
            video_encode_dst = VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR,
            acceleration_structure_input = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            acceleration_structure_storage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
            shader_binding_table = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
            sampler_descriptor = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
            resource_descriptor = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
            push_descriptor = VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT,
            micromap_input = VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT,
            micromap_storage = VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT,
#ifdef VK_ENABLE_BETA_EXTENSIONS
            execution_graph_scratch = VK_BUFFER_USAGE_EXECUTION_GRAPH_SCRATCH_BIT_AMDX,
#endif
            // Common combinations; upload adds transfer_dst, download adds
            // transfer_src, and transfer adds both transfer directions.
            storage_upload = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            uniform_upload = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            indirect_storage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            storage_transfer = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            transfer = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            vertex_storage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            storage_download = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            acceleration_input = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            acceleration_storage_input = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            index_storage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            vertex_upload = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            indirect_storage_transfer = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            vertex_storage_transfer = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            index_storage_transfer = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };

        VkBuffer _{};
        VkBufferCreateInfo create_info{};
        VmaAllocation allocation{};
        VmaAllocator allocator{};

        [[nodiscard]] mapping map() const;

        static buffer_builder builder(vma_memory_allocator &allocator);

        static buffer build(vma_memory_allocator &allocator);

        void destroy();

        void flush(size_t offset = 0, size_t size = VK_WHOLE_SIZE);
        void invalidate(size_t offset = 0, size_t size = VK_WHOLE_SIZE);

        operator VkBuffer() const {
            return _;
        }
    };

    struct mesh_buffers {
        buffer vertex_buffer;
        std::optional<buffer> index_buffer;
        VkIndexType index_type{VK_INDEX_TYPE_UINT32};
    };

    struct image {
        using flags = vkz::image_flags;
        enum class aspect : VkImageAspectFlags {
            none = VK_IMAGE_ASPECT_NONE,
            color = VK_IMAGE_ASPECT_COLOR_BIT,
            depth = VK_IMAGE_ASPECT_DEPTH_BIT,
            stencil = VK_IMAGE_ASPECT_STENCIL_BIT,
            metadata = VK_IMAGE_ASPECT_METADATA_BIT,
            plane_0 = VK_IMAGE_ASPECT_PLANE_0_BIT,
            plane_1 = VK_IMAGE_ASPECT_PLANE_1_BIT,
            plane_2 = VK_IMAGE_ASPECT_PLANE_2_BIT,
            memory_plane_0 = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT,
            memory_plane_1 = VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT,
            memory_plane_2 = VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT,
            memory_plane_3 = VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT,
            depth_stencil = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        };

        enum class usage : VkImageUsageFlags {
            none = 0,
            transfer_src = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            transfer_dst = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            sampled = VK_IMAGE_USAGE_SAMPLED_BIT,
            storage = VK_IMAGE_USAGE_STORAGE_BIT,
            color = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            depth_stencil = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            transient = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
            input = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            video_decode_src = VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
            video_decode_dst = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR,
            video_decode_dpb = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
            video_encode_src = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR,
            video_encode_dst = VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR,
            video_encode_dpb = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR,
            fragment_density_map = VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT,
            fragment_shading_rate = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
            host_transfer = VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT,
            attachment_feedback_loop = VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT,
            invocation_mask = VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI,
            sample_weight = VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM,
            sample_block_match = VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM,

            // Common combinations; upload adds transfer_dst, download adds
            // transfer_src, and transfer adds both transfer directions.
            sampled_storage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            sampled_storage_transfer = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            sampled_upload = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            color_sampled = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            color_download = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            sampled_storage_download = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            sampled_transfer = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            color_input_sampled_download = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            color_sampled_storage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            depth_stencil_sampled = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            sampled_storage_upload = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            storage_download = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            transfer = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            color_input_sampled = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            color_sampled_upload = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            storage_transfer = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        };

        VkImage handle{};
        VkImageCreateInfo create_info{};
        VmaAllocation allocation{};
        VmaAllocator allocator{};
        VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};

        static image_builder builder(vma_memory_allocator &allocator);

        static image build(vma_memory_allocator &allocator);

        void destroy();

        operator VkImage() const {
            return handle;
        }
    };

    struct image_view {
        using flags = vkz::image_view_flags;
        VkImageView handle{};
        VkImageViewCreateInfo create_info{};
        VkDevice device{};

        static image_view_builder builder(vkz::device device);

        static image_view build(vkz::device device, const vkz::image &image,
                                VkFormat format = VK_FORMAT_UNDEFINED);

        void destroy();

        operator VkImageView() const {
            return handle;
        }
    };

    struct sub_resource {
        image::aspect aspect_mask{image::aspect::color};
        uint32_t base_mip_level{0};
        uint32_t level_count{1};
        uint32_t base_array_layer{0};
        uint32_t layer_count{1};
    };

    struct sampler {
        using flags = vkz::sampler_flags;
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

    struct mapping {
        friend struct buffer;

        friend class staging_buffer;

        void *_{};

        template<typename T>
        T *as() {
            return reinterpret_cast<T *>(_);
        }

        void unmap() {
            if (_) {
                assert(allocation != nullptr && allocator != nullptr);
                vmaUnmapMemory(allocator, allocation);
                _ = nullptr;
            }
        }

        operator void *() const {
            return _;
        }

    private:
        VmaAllocation allocation{};
        VmaAllocator allocator{};

    };

    struct vma_memory_allocator {
        VkInstance instance{VK_NULL_HANDLE};
        device device;
        VmaAllocator allocator{VK_NULL_HANDLE};

        static vma_memory_allocator create(const context &context, VmaAllocatorCreateFlags flags = 0);

        static vma_memory_allocator create_not_owned(VmaAllocator allocator);

        void destroy();

        buffer allocate(VkBufferCreateInfo create_info, VmaMemoryUsage usage);

        image allocate(VkImageCreateInfo create_info, VmaMemoryUsage usage = VMA_MEMORY_USAGE_GPU_ONLY);

        buffer_builder make_buffer_builder();

        image_builder make_image_builder();

        void deallocate(buffer buffer);

        void deallocate(image image);

    private:
        bool owns_allocator{};
    };

    // VMA-backed memory for objects (such as video sessions) with explicit requirements.
    struct device_memory {
        static device_memory allocate(vma_memory_allocator &allocator, const VkMemoryRequirements &requirements);
        void destroy();

        VkDeviceMemory handle{};
        VkDeviceSize offset{};
        VkDeviceSize size{};
        VmaAllocation allocation{};
        VmaAllocator allocator{};
    };

    class staging_buffer {
    public:
        class borrowed_memory {
            friend class staging_buffer;

        public:
            borrowed_memory() = default;

            borrowed_memory(const borrowed_memory &) = delete;

            borrowed_memory &operator=(const borrowed_memory &) = delete;

            borrowed_memory(borrowed_memory &&other) noexcept;

            borrowed_memory &operator=(borrowed_memory &&other) = delete;

            [[nodiscard]] void *data() const;

            template<typename T>
            [[nodiscard]] T *as() const {
                return static_cast<T *>(data());
            }

            [[nodiscard]] VkDeviceSize offset() const;

            [[nodiscard]] VkDeviceSize size() const;

            [[nodiscard]] buffer source_buffer() const;

            [[nodiscard]] bool valid() const;

            void
            copy_from(const void *source, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize destination_offset = 0) const;

            void
            upload(const void *source, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize destination_offset = 0) const;

            void copy_to(
                    VkCommandBuffer command_buffer,
                    VkBuffer destination,
                    VkDeviceSize destination_offset = 0,
                    VkDeviceSize size = VK_WHOLE_SIZE,
                    VkDeviceSize source_offset = 0) const;

            void copy_to(
                    VkCommandBuffer command_buffer,
                    const buffer &destination,
                    VkDeviceSize destination_offset = 0,
                    VkDeviceSize size = VK_WHOLE_SIZE,
                    VkDeviceSize source_offset = 0) const;

        private:
            borrowed_memory(staging_buffer *owner, uint64_t id, VkDeviceSize offset, VkDeviceSize size);

            staging_buffer *owner_{};
            uint64_t id_{};
            VkDeviceSize offset_{};
            VkDeviceSize size_{};
        };

        staging_buffer(vma_memory_allocator &allocator, VkDeviceSize capacity);

        staging_buffer(const staging_buffer &) = delete;

        staging_buffer &operator=(const staging_buffer &) = delete;

        staging_buffer(staging_buffer &&) = delete;

        staging_buffer &operator=(staging_buffer &&) = delete;

        [[nodiscard]] borrowed_memory borrow(VkDeviceSize size, VkDeviceSize alignment = 1);

        void return_memory(borrowed_memory &memory);

        void reset();

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

        [[nodiscard]] bool owns(const borrowed_memory &memory) const;

        void validate_subrange(const borrowed_memory &memory, VkDeviceSize offset, VkDeviceSize size) const;

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
        explicit buffer_builder(vma_memory_allocator &allocator);

        buffer_builder &flags(vkz::buffer::flags value);

        buffer_builder &size(VkDeviceSize value);

        buffer_builder &usage(buffer::usage value);

        buffer_builder &sharing_mode(VkSharingMode value);

        buffer_builder &queue_families(const uint32_t *indices, uint32_t count);

        buffer_builder &memory_usage(VmaMemoryUsage value);

        buffer_builder &next(const void *value);

        buffer_builder &name(const std::string &name);

        [[nodiscard]] const VkBufferCreateInfo &create_info() const;

        [[nodiscard]] buffer build() const;

    private:
        vma_memory_allocator *allocator_{};
        VkBufferCreateInfo create_info_{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        VmaMemoryUsage memory_usage_{VMA_MEMORY_USAGE_AUTO};
        void *data_{};
        VkDeviceSize data_size_{};
        std::string name_;

    };

    class image_builder {
    public:
        explicit image_builder(vma_memory_allocator &allocator);

        image_builder &flags(vkz::image::flags value);

        image_builder &type(VkImageType value);

        image_builder &format(VkFormat value);

        image_builder &extent(VkExtent3D value);

        image_builder &extent(uint32_t width, uint32_t height, uint32_t depth = 1);

        image_builder &mip_levels(uint32_t value);

        image_builder &array_layers(uint32_t value);

        image_builder &samples(vkz::sample_count value);

        image_builder &tiling(VkImageTiling value);

        image_builder &usage(image::usage value);

        image_builder &sharing_mode(VkSharingMode value);

        image_builder &queue_families(const uint32_t *indices, uint32_t count);

        image_builder &initial_layout(VkImageLayout value);

        image_builder &memory_usage(VmaMemoryUsage value);

        image_builder &next(const void *value);

        [[nodiscard]] const VkImageCreateInfo &create_info() const;

        [[nodiscard]] image build() const;

    private:
        vma_memory_allocator *allocator_{};
        VkImageCreateInfo create_info_{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        VmaMemoryUsage memory_usage_{VMA_MEMORY_USAGE_GPU_ONLY};
    };

    class image_view_builder {
    public:
        explicit image_view_builder(vkz::device device);

        image_view_builder &flags(vkz::image_view::flags value);

        image_view_builder &image(const vkz::image &value);

        image_view_builder &view_type(VkImageViewType value);

        image_view_builder &format(VkFormat value);

        image_view_builder &components(VkComponentMapping value);

        image_view_builder &subresource_range(VkImageSubresourceRange value);

        image_view_builder &aspect_mask(image::aspect value);

        image_view_builder &base_mip_level(uint32_t value);

        image_view_builder &level_count(uint32_t value);

        image_view_builder &base_array_layer(uint32_t value);

        image_view_builder &layer_count(uint32_t value);

        image_view_builder &next(const void *value);

        [[nodiscard]] const VkImageViewCreateInfo &create_info() const;

        [[nodiscard]] image_view build() const;

    private:
        vkz::device device_{};
        VkImageViewCreateInfo create_info_{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    };

    class sampler_builder {
    public:
        explicit sampler_builder(vkz::device device);

        sampler_builder &flags(vkz::sampler::flags value);

        sampler_builder &mag_filter(VkFilter value);

        sampler_builder &min_filter(VkFilter value);

        sampler_builder &mipmap_mode(VkSamplerMipmapMode value);

        sampler_builder &address_mode(VkSamplerAddressMode value);

        sampler_builder &address_mode_u(VkSamplerAddressMode value);

        sampler_builder &address_mode_v(VkSamplerAddressMode value);

        sampler_builder &address_mode_w(VkSamplerAddressMode value);

        sampler_builder &mip_lod_bias(float value);

        sampler_builder &anisotropy(bool enabled, float max = 1.0f);

        sampler_builder &compare(bool enabled, VkCompareOp op = VK_COMPARE_OP_ALWAYS);

        sampler_builder &min_lod(float value);

        sampler_builder &max_lod(float value);

        sampler_builder &border_color(VkBorderColor value);

        sampler_builder &unnormalized_coordinates(bool enabled);

        sampler_builder &next(const void *value);

        [[nodiscard]] const VkSamplerCreateInfo &create_info() const;

        [[nodiscard]] sampler build() const;

    private:
        vkz::device device_{};
        VkSamplerCreateInfo create_info_{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    };
}

template <> struct enable_bitmask_operators<vkz::buffer::usage> {
    static constexpr bool enable = true;
};

template <> struct enable_bitmask_operators<vkz::image::usage> {
    static constexpr bool enable = true;
};

template <> struct enable_bitmask_operators<vkz::image::aspect> {
    static constexpr bool enable = true;
};
