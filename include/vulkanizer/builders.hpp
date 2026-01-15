#pragma once

#include "GraphicsPipelineBuilder.hpp"
#include "DescriptorSetBuilder.hpp"

namespace vkz {

    GraphicsPipelineBuilder graphics_pipeline_builder(Device device);

    DescriptorSetLayoutBuilder descriptorset_layout_builder(Device device);
}