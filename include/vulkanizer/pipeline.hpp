#pragma once

#include "vkz.hpp"
#include "descriptors.hpp"
#include <volk.h>

namespace vkz {

    struct pipeline {
        VkPipeline handle{};
        VkPipelineLayout layout{};
        VkPipelineBindPoint bind_point{VK_PIPELINE_BIND_POINT_COMPUTE};
        device device;

        std::vector<descriptor_set> descriptor_sets;

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