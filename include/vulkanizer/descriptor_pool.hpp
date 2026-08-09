#pragma once

#include "vkz.hpp"

#include <initializer_list>
#include <vector>
#include <span>

namespace vkz {

    class descriptor_pool {
    public:
        descriptor_pool() = default;

        descriptor_pool(device device, uint32_t max_sets,
                        std::initializer_list<VkDescriptorPoolSize> pool_sizes,
                        VkDescriptorPoolCreateFlags flags = 0);

        std::vector<VkDescriptorSet> allocate(std::span<VkDescriptorSetLayout> layouts) const;

        VkDescriptorSet allocate(VkDescriptorSetLayout layout) const;

        std::vector<VkDescriptorSet> allocate_n(VkDescriptorSetLayout layout, size_t count) const;

        void free(VkDescriptorSet set) const;

        void free(const std::span<VkDescriptorSet>& sets) const;

        void reset() const;

        void destroy();

        [[nodiscard]] VkDescriptorPool handle() const;

        operator VkDescriptorPool() const;

    private:
        device device_;
        VkDescriptorPool _pool{};
    };

}
