#include "vulkanizer/descriptors.hpp"
#include "vulkanizer/status.hpp"
#include "vulkanizer/detail/functions.hpp"

#include <limits>

namespace vkz {
    descriptor_pool::descriptor_pool(
            vkz::device device,
            uint32_t max_sets,
            std::initializer_list<VkDescriptorPoolSize> pool_sizes,
            VkDescriptorPoolCreateFlags flags)
        : device_{device} {
        if (!device_.logical) {
            VKZ_THROW("Cannot create a descriptor pool with a null logical device")
        }
        if (!max_sets) {
            VKZ_THROW("Descriptor pool max_sets must be greater than zero")
        }
        if (pool_sizes.size() > std::numeric_limits<uint32_t>::max()) {
            VKZ_THROW("Descriptor pool size count exceeds the Vulkan uint32_t limit")
        }

        const VkDescriptorPoolCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .maxSets = max_sets,
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes = pool_sizes.begin(),
        };
        VKZ_CHECK_VULKAN(vkCreateDescriptorPool(device_.logical, &create_info, nullptr, &_pool));
    }

    std::vector<descriptor_set> descriptor_pool::allocate(std::span<descriptor_set_layout> layouts) {
        if (!_pool) {
            VKZ_THROW("Cannot allocate descriptor sets from a null descriptor pool")
        }
        if (layouts.empty()) {
            return {};
        }
        if (layouts.size() > std::numeric_limits<uint32_t>::max()) {
            VKZ_THROW("Descriptor set layout count exceeds the Vulkan uint32_t limit")
        }

        auto vulkan_layouts = map_range(layouts, [](auto layout){ return layout.handle; });
        const VkDescriptorSetAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = _pool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = vulkan_layouts.data(),
        };
        std::vector<VkDescriptorSet> vulkan_sets(layouts.size());
        VKZ_CHECK_VULKAN(vkAllocateDescriptorSets(device_.logical, &allocate_info, vulkan_sets.data()));
        return map_range(vulkan_sets, [this](auto set){ return descriptor_set{ set, this }; });
    }

    descriptor_set descriptor_pool::allocate(descriptor_set_layout layout) {
        if (!layout.handle) {
            VKZ_THROW("Cannot allocate a descriptor set with a null layout")
        }
        std::span layouts{&layout, std::size_t{1}};
        return allocate(layouts).front();
    }

    std::vector<descriptor_set> descriptor_pool::allocate_n(descriptor_set_layout layout, size_t count) {
        if (!layout.handle && count) {
            VKZ_THROW("Cannot allocate descriptor sets with a null layout")
        }
        if (count > std::numeric_limits<uint32_t>::max()) {
            VKZ_THROW("Descriptor set count exceeds the Vulkan uint32_t limit")
        }
        if (!count) {
            return {};
        }

        std::vector<descriptor_set_layout> layouts(count, layout);
        return allocate(layouts);
    }

    void descriptor_pool::free(descriptor_set set) {
        if (!set) {
            return;
        }
        free(std::span{&set, std::size_t{1}});
    }

    void descriptor_pool::free(const std::span<descriptor_set>& sets) {
        if (!_pool) {
            VKZ_THROW("Cannot free descriptor sets from a null descriptor pool")
        }
        if (sets.empty()) {
            return;
        }
        if (sets.size() > std::numeric_limits<uint32_t>::max()) {
            VKZ_THROW("Descriptor set count exceeds the Vulkan uint32_t limit")
        }

        auto vulkan_sets = map_range(sets, [](auto set){ return set.handle; });
        VKZ_CHECK_VULKAN(vkFreeDescriptorSets(
            device_.logical,
            _pool,
            static_cast<uint32_t>(sets.size()),
            vulkan_sets.data()));
    }

    void descriptor_pool::reset() {
        if (!_pool) {
            return;
        }
        VKZ_CHECK_VULKAN(vkResetDescriptorPool(device_.logical, _pool, 0));
    }

    void descriptor_pool::destroy() {
        if (_pool) {
            vkDestroyDescriptorPool(device_.logical, _pool, nullptr);
            _pool = VK_NULL_HANDLE;
        }
    }

    VkDescriptorPool descriptor_pool::handle() const {
        return _pool;
    }

    descriptor_pool::operator VkDescriptorPool() const {
        return _pool;
    }
}
