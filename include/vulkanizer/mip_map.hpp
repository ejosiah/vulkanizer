#pragma once

#include "commands.hpp"
#include "memory.hpp"

namespace vkz {

    void generate_mip_maps(
        VkCommandBuffer command_buffer,
        image& image,
        double k = 1.0,
        VkFilter filter = VK_FILTER_LINEAR);

    void generate_mip_maps(
        command_pool& commands,
        image& image,
        double k = 1.0,
        VkFilter filter = VK_FILTER_LINEAR);

}
