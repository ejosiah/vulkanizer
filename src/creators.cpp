#include "vulkanizer/creators.hpp"
#include "vulkanizer/io.hpp"

namespace vkz {

    VkShaderModule create_shader_module(vkz::device device, const std::string &path) {
        auto data = loadFile(path);
        return create_shader_module(device, data);
    }

    VkShaderModule create_shader_module(vkz::device device, const byte_string &data) {
        auto ptr = reinterpret_cast<uint32_t *>(const_cast<char *>(data.data()));
        return create_shader_module(device, std::span<uint32_t>{
                ptr,
                data.size() / sizeof(uint32_t),
        });
    }

    VkShaderModule create_shader_module(vkz::device device, const std::vector<uint32_t> &data) {
        auto ptr = reinterpret_cast<uint32_t *>(const_cast<uint32_t *>(data.data()));
        return create_shader_module(device, {ptr, data.size()});
    }

    VkShaderModule create_shader_module(vkz::device device, std::span<uint32_t> data) {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = data.size() * sizeof(uint32_t);
        create_info.pCode = data.data();

        VkShaderModule module;
        auto status = vkCreateShaderModule(device.logical, &create_info, nullptr, &module);
        VKZ_REPORT_ERROR(status, "Failed to create shader module")

        return module;
    }
}