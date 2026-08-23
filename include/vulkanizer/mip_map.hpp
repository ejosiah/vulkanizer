#pragma once

#include "commands.hpp"
#include "memory.hpp"

namespace vkz {

    void generate_mip_maps(VkCommandBuffer command_buffer, image& image, VkFilter filter = VK_FILTER_LINEAR);

}
