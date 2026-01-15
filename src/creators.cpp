#include "vulkanizer/creators.hpp"
#include "vulkanizer/io.hpp"

namespace vkz {

    VkShaderModule createShaderModule(Device device, const std::string &path) {
        auto data = loadFile(path);
        return createShaderModule(device, data);
    }

    VkShaderModule createShaderModule(Device device, const byte_string &data) {
        auto ptr = reinterpret_cast<uint32_t *>(const_cast<char *>(data.data()));
        return createShaderModule(device, std::span<uint32_t>{
                ptr,
                data.size() / sizeof(uint32_t),
        });
    }

    VkShaderModule createShaderModule(Device device, const std::vector<uint32_t> &data) {
        auto ptr = reinterpret_cast<uint32_t *>(const_cast<uint32_t *>(data.data()));
        return createShaderModule(device, {ptr, data.size()});
    }

    VkShaderModule createShaderModule(Device device, std::span<uint32_t> data) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = data.size() * sizeof(uint32_t);
        createInfo.pCode = data.data();

        VkShaderModule module;
        auto status = vkCreateShaderModule(device.logical, &createInfo, nullptr, &module);
        VKZ_REPORT_ERROR(status, "Failed to create shader module")

        return module;
    }
}