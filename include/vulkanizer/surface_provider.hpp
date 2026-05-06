#pragma once

#include "vkz.hpp"

namespace vkz {

    class surface_provider {
    public:
        virtual ~surface_provider() = default;

        virtual VkSurfaceKHR operator()(VkInstance instance) const = 0;
    };
}
