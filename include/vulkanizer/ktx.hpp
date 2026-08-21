#pragma once

#include "memory.hpp"

#include <filesystem>
#include <vector>

namespace vkz {

    [[nodiscard]] texture texture_from_ktx(const std::filesystem::path& path, vma_memory_allocator& allocator,
                                           VkQueue upload_queue, uint32_t upload_queue_family, VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                           bool enable_anisotropy = false);

    namespace ktx {
        struct texture_snapshot {
            VkImageCreateInfo create_info{};
            std::vector<uint8_t> pixels;
        };

        texture_snapshot read(vma_memory_allocator& allocator, VkQueue queue, uint32_t queue_family_index,
                              const texture& texture);

        void save(const std::filesystem::path& path, const texture_snapshot& snapshot);

        void save(const std::filesystem::path& path, vma_memory_allocator& allocator, VkQueue queue,
                  uint32_t queue_family_index, const texture& texture);
    }
}
