#pragma once


#include "status.hpp"
#include <vulkan/vulkan.h>

namespace vkz {

    struct Device {
        VkPhysicalDevice physical;
        VkDevice logical;
    };

    struct ShaderInfo{
        VkShaderModule module{};
        VkShaderStageFlagBits stage;
        const char*  entry = "main";
    };

    template<VkObjectType objectType>
    inline void setName(Device device, const std::string& objectName, void* ptr)  {
#ifndef NDEBUG
        auto objectHandle = (uint64_t)ptr;
        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.pObjectName = objectName.c_str();
        nameInfo.objectType = objectType;
        nameInfo.objectHandle = objectHandle;
        vkSetDebugUtilsObjectNameEXT(device.logical, &nameInfo);
#endif
    }
}