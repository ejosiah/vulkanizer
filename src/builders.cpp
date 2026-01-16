#include "vulkanizer/builders.hpp"

vkz::GraphicsPipelineBuilder vkz::graphics_pipeline_builder(vkz::Device device) {
    return vkz::GraphicsPipelineBuilder(device);
}

vkz::ComputePipelineBuilder vkz::compute_pipeline_bulder(vkz::Device device) {
    return vkz::ComputePipelineBuilder(device);
}

vkz::DescriptorSetLayoutBuilder vkz::descriptorset_layout_builder(vkz::Device device) {
    return vkz::DescriptorSetLayoutBuilder(device);
}
