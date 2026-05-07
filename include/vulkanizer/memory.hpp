#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cassert>
#include <cinttypes>
#include <map>

namespace vkz {

    struct Buffer {
        VkBuffer _{};
        VkBufferCreateInfo info{};
        VmaAllocation allocation{};

        operator VkBuffer() const {
            return _;
        }
    };

    struct Image {
        VkImage handle{};
        VkImageCreateInfo info{};
        VmaAllocation allocation{};
        VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};

        operator VkImage() const {
            return handle;
        }
    };

    struct ImageView {
        VkImageView handle{};
        VkImageViewCreateInfo info{};

        operator VkImageView() const {
            return handle;
        }
    };

    struct Sampler {
        VkSampler handle{};
        VkSamplerCreateInfo info{};

        operator VkSampler() const {
            return handle;
        }
    };

    struct Texture {
        Image image;
        ImageView imageView;
    };

    struct Mapping {
        friend class VulkanGraphicsService;
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

    struct VmaMemoryAllocator {
        VkInstance instance{VK_NULL_HANDLE};
        VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
        VkDevice device{VK_NULL_HANDLE};
        VmaAllocator allocator{VK_NULL_HANDLE};

        void init();

        void destroy();

        Buffer allocate(VkBufferCreateInfo createInfo, VmaMemoryUsage usage);

        Image allocate(VkImageCreateInfo createInfo, VmaMemoryUsage usage = VMA_MEMORY_USAGE_GPU_ONLY);

        void deallocate(Buffer buffer);

        void deallocate(Image image);
    };
}