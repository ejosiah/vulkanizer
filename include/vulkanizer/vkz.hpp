#pragma once


#include "types.hpp"
#include "status.hpp"
#include <volk.h>

namespace vkz {

    struct device {
        VkPhysicalDevice physical{};
        VkDevice logical{};
    };

    struct shader_info{
        VkShaderModule module{};
        VkShaderStageFlagBits stage{};
        const char* entry{"main"};
    };

    template<VkObjectType object_type>
    inline void set_name(vkz::device device, const std::string& object_name, void* ptr)  {
#ifndef NDEBUG
        auto object_handle = (uint64_t)ptr;
        VkDebugUtilsObjectNameInfoEXT name_info{};
        name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        name_info.pObjectName = object_name.c_str();
        name_info.objectType = object_type;
        name_info.objectHandle = object_handle;
        vkSetDebugUtilsObjectNameEXT(device.logical, &name_info);
#endif
    }
}
