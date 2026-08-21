#pragma once

#include <cinttypes>
#include <concepts>
#include <vector>
#include <volk.h>

namespace vkz {
    using real = float;
    using uint = unsigned int;
    using int32 = int32_t;
    using uint32 = uint32_t;
    using int64 = uint64_t;
    using uint64 = uint64_t;
    using Flags = unsigned int;
    using byte_string = std::vector<char>;
    using ubyte_string = std::vector<unsigned char>;

    template <typename T>
    concept vulkan_structure =
        requires(T t) {
            { t.sType } -> std::convertible_to<VkStructureType>;
            { t.pNext } -> std::convertible_to<const void*>;
        };
}
