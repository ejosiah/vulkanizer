#pragma once

#include "vkz.hpp"

#include <initializer_list>
#include <vector>
#include <span>

namespace vkz {

    struct descriptor_set_layout {
        VkDescriptorSetLayout handle{};
        device device;

        operator bool() const {
            return handle != nullptr;
        }

        void destroy() const {
            if(handle) {
                vkDestroyDescriptorSetLayout(device.logical, handle, nullptr);
            }
        }
    };

    struct descriptor_set;

    class descriptor_pool {
    public:
        descriptor_pool() = default;

        descriptor_pool(device device, uint32_t max_sets,
                        std::initializer_list<VkDescriptorPoolSize> pool_sizes,
                        VkDescriptorPoolCreateFlags flags = 0);

        [[nodiscard]] std::vector<descriptor_set> allocate(std::span<descriptor_set_layout> layouts);

        descriptor_set allocate(descriptor_set_layout layout);

        std::vector<descriptor_set> allocate_n(descriptor_set_layout layout, size_t count);

        void free(descriptor_set set);

        void free(const std::span<descriptor_set>& sets);

        void reset();

        void destroy();

        [[nodiscard]] VkDescriptorPool handle() const;

        operator VkDescriptorPool() const;

    private:
        device device_;
        VkDescriptorPool _pool{};
    };

    struct descriptor_set {
        VkDescriptorSet handle{};
        descriptor_pool* pool{};

        operator bool() const {
            return pool != nullptr && handle != nullptr;
        }

        void free() const {
            assert(pool);
            pool->free(*this);
        }
    };

}
