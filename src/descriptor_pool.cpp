#include "vulkanizer/descriptor_pool.hpp"

#include "vulkanizer/status.hpp"

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

    std::vector<VkDescriptorSet> descriptor_pool::allocate(std::span<VkDescriptorSetLayout> layouts) const {
        if (!_pool) {
            VKZ_THROW("Cannot allocate descriptor sets from a null descriptor pool")
        }
        if (layouts.empty()) {
            return {};
        }
        if (layouts.size() > std::numeric_limits<uint32_t>::max()) {
            VKZ_THROW("Descriptor set layout count exceeds the Vulkan uint32_t limit")
        }

        const VkDescriptorSetAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = _pool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
        };
        std::vector<VkDescriptorSet> sets(layouts.size());
        VKZ_CHECK_VULKAN(vkAllocateDescriptorSets(device_.logical, &allocate_info, sets.data()));
        return sets;
    }

    VkDescriptorSet descriptor_pool::allocate(VkDescriptorSetLayout layout) const {
        if (!layout) {
            VKZ_THROW("Cannot allocate a descriptor set with a null layout")
        }
        std::span layouts{&layout, std::size_t{1}};
        return allocate(layouts).front();
    }

    std::vector<VkDescriptorSet> descriptor_pool::allocate_n(VkDescriptorSetLayout layout, size_t count) const {
        if (!layout && count) {
            VKZ_THROW("Cannot allocate descriptor sets with a null layout")
        }
        if (count > std::numeric_limits<uint32_t>::max()) {
            VKZ_THROW("Descriptor set count exceeds the Vulkan uint32_t limit")
        }
        if (!count) {
            return {};
        }

        std::vector<VkDescriptorSetLayout> layouts(count, layout);
        return allocate(layouts);
    }

    void descriptor_pool::free(VkDescriptorSet set) const {
        if (!set) {
            return;
        }
        free(std::span{&set, std::size_t{1}});
    }

    void descriptor_pool::free(const std::span<VkDescriptorSet>& sets) const {
        if (!_pool) {
            VKZ_THROW("Cannot free descriptor sets from a null descriptor pool")
        }
        if (sets.empty()) {
            return;
        }
        if (sets.size() > std::numeric_limits<uint32_t>::max()) {
            VKZ_THROW("Descriptor set count exceeds the Vulkan uint32_t limit")
        }
        VKZ_CHECK_VULKAN(vkFreeDescriptorSets(
            device_.logical,
            _pool,
            static_cast<uint32_t>(sets.size()),
            sets.data()));
    }

    void descriptor_pool::reset() const {
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
