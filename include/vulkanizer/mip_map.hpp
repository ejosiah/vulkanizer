#pragma once

#include "memory.hpp"

namespace vkz {

    void generate_mip_maps(VkCommandBuffer command_buffer, image& image, double k = 1.0, VkFilter filter = VK_FILTER_LINEAR);

}
