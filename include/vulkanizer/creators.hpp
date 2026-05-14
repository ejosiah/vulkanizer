#pragma once

#include "vkz.hpp"
#include "types.hpp"

#include <string>
#include <span>

namespace vkz {

    VkShaderModule create_shader_module(vkz::device device, const std::string& path);

    VkShaderModule create_shader_module(vkz::device device, const byte_string& data);

    VkShaderModule create_shader_module(vkz::device device, const std::vector<uint32_t> &data);

    VkShaderModule create_shader_module(vkz::device device, std::span<uint32_t> data);

}