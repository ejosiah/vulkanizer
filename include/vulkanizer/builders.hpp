#pragma once

#include "GraphicsPipelineBuilder.hpp"
#include "ComputePipelineBuilder.hpp"
#include "DescriptorSetBuilder.hpp"

namespace vkz {

    GraphicsPipelineBuilder graphics_pipeline_builder(Device device);

    ComputePipelineBuilder compute_pipeline_bulder(Device device);

    DescriptorSetLayoutBuilder descriptorset_layout_builder(Device device);
}