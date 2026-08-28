#pragma once

#include "vkz.hpp"
#include "memory.hpp"
#include "texture.hpp"

#include <initializer_list>
#include <vector>
#include <span>
#include <variant>

namespace vkz {

   struct buffer_descriptor {
       buffer buffer;
       VkDeviceSize start{0};
       VkDeviceSize end{VK_WHOLE_SIZE};
       uint32_t binding{~0u};
   };

   struct buffer_element_descriptor {
       buffer_descriptor descriptor;
       uint32_t array_element_location{~0u};
   };

   struct buffer_array_descriptor : std::vector<buffer_descriptor> {
       uint32_t binding{~0u};
   };

   struct ubo_descriptor {
       buffer buffer;
       VkDeviceSize start{0};
       VkDeviceSize end{VK_WHOLE_SIZE};
       uint32_t binding{~0u};
   };

    struct ubo_element_descriptor {
        ubo_descriptor descriptor;
        uint32_t array_element_location{~0u};
    };

    struct ubo_array_descriptor : std::vector<ubo_descriptor> {
        uint32_t binding{~0u};
    };

   struct image_descriptor {
       image_view view;
       VkImageLayout layout{VK_IMAGE_LAYOUT_GENERAL};
       uint32_t binding{~0u};
   };

   struct image_element_descriptor {
       image_descriptor image;
       uint32_t array_element_location{~0u};
   };

    struct image_array_descriptor : std::vector<image_descriptor> {
        uint32_t binding{~0u};
    };

   struct texture_descriptor {
       image_view view;
       sampler sampler;
       VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
       uint32_t binding{~0u};
   };

    template<typename Descriptor, typename Resource>
    Descriptor descriptor(const Resource& resource, uint32_t binding) = delete;

    template<>
    inline buffer_descriptor descriptor<buffer_descriptor, buffer>(const buffer& buffer, uint32_t binding) {
        return {
            .buffer = buffer,
            .binding = binding,
        };
    }

    template<>
    inline ubo_descriptor descriptor<ubo_descriptor, buffer>(const buffer& buffer, uint32_t binding) {
        return {
            .buffer = buffer,
            .binding = binding,
        };
    }

    template<>
    inline image_descriptor descriptor<image_descriptor, texture>(const texture& texture, uint32_t binding) {
        return {
            .view = texture.image_view,
            .layout = texture.image.layout,
            .binding = binding,
        };
    }

    template<>
    inline texture_descriptor descriptor<texture_descriptor, texture>(const texture& texture, uint32_t binding) {
        return {
            .view = texture.image_view,
            .sampler = texture.sampler,
            .layout = texture.image.layout,
            .binding = binding,
        };
    }

   struct texture_element_descriptor {
       texture_descriptor texture;
       uint32_t array_element_location{~0u};
   };

    struct texture_array_descriptor : std::vector<texture_descriptor> {
        uint32_t binding{~0u};
    };

    using descriptor_t = std::variant<buffer_descriptor,
                                buffer_element_descriptor,
                                buffer_array_descriptor,
                                ubo_descriptor,
                                ubo_element_descriptor,
                                ubo_array_descriptor,
                                image_descriptor,
                                image_element_descriptor,
                                image_array_descriptor,
                                texture_descriptor,
                                texture_element_descriptor,
                                texture_array_descriptor>;

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

    struct descriptor_bindings {
        descriptor_set descriptor_set;
        std::vector<descriptor_t> bindings;
    };

    void update_descriptor(device device, descriptor_bindings bindings);

    void update_descriptors(device device, std::initializer_list<descriptor_bindings> bindings);
}
