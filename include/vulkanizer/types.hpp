#pragma once

#include <volk.h>

#include <cinttypes>
#include <concepts>
#include <vector>
#include <glm/glm.hpp>

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

    using offset2d = glm::ivec2;
    using extent2d = glm::uvec2;

    using offset3d = glm::ivec3;
    using extent3d = glm::uvec3;

    using color = glm::vec4;
    using icolor = glm::ivec4;
    using ucolor = glm::uvec4;

    template <typename T>
    concept vulkan_structure =
        requires(T t) {
            { t.sType } -> std::convertible_to<VkStructureType>;
            { t.pNext } -> std::convertible_to<const void*>;
        };

    struct rect2d {
        offset2d origin;
        extent2d dimensions;
    };

    struct rect3d {
        offset3d origin;
        extent3d dimensions;
    };

    struct clear_color {
        union {
            color fcolor;
            icolor icolor;
            ucolor ucolor;
        } value{};
    };

}
