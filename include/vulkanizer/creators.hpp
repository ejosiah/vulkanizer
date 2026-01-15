#pragma once

#include "vkz.hpp"
#include "types.hpp"

#include <string>
#include <span>

namespace vkz {

    VkShaderModule createShaderModule(Device device, const std::string& path);

    VkShaderModule createShaderModule(Device device, const byte_string& data);

    VkShaderModule createShaderModule(Device device, const std::vector<uint32_t> &data);

    VkShaderModule createShaderModule(Device device, std::span<uint32_t> data);

}