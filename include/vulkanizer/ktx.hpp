#pragma once

#include "memory.hpp"

#include <filesystem>

namespace vkz {

    [[nodiscard]] texture texture_from_ktx(const std::filesystem::path& path, vma_memory_allocator& allocator,
                                           VkQueue upload_queue, uint32_t upload_queue_family, VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                           bool enable_anisotropy = false);
}
