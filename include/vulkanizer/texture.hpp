#pragma once

#include "memory.hpp"

#include <filesystem>

namespace vkz {

    struct texture {
        image image;
        image_view image_view;
        sampler sampler;

        void destroy();
    };

    texture load(vma_memory_allocator& allocator, VkQueue upload_queue, uint32_t upload_queue_family,
                     const std::filesystem::path &path, VkFormat format, VkSamplerAddressMode addressMode,
                     bool flip_uv = false);
}