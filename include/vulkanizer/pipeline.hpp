#pragma once

#include "vkz.hpp"
#include <volk.h>

namespace vkz {

    struct pipeline {
        VkPipeline handle{};
        VkPipelineLayout layout{};
        device device;

        void destroy() const {
            if(handle) {
                vkDestroyPipeline(device.logical, handle, nullptr);
            }
            if(layout) {
                vkDestroyPipelineLayout(device.logical, layout, nullptr);
            }
        }
    };

}