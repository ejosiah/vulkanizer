#include "vulkanizer/ComputePipelineBuilder.hpp"

namespace vkz {

    ComputePipelineLayoutBuilder::ComputePipelineLayoutBuilder(Device device, ComputePipelineBuilder *builder)
            : ComputePipelineBuilder(device, builder) {

    }

    ComputePipelineLayoutBuilder &ComputePipelineLayoutBuilder::addDescriptorSetLayout(VkDescriptorSetLayout layout) {
        _descriptorSetLayouts.push_back(layout);
        return *this;
    }

    ComputePipelineLayoutBuilder &
    ComputePipelineLayoutBuilder::addPushConstantRange(VkShaderStageFlags stage, uint32_t offset, uint32_t size) {
        _ranges.push_back({stage, offset, size});
        return *this;
    }

    VkPipelineLayout ComputePipelineLayoutBuilder::buildPipelineLayout() const {

        VkPipelineLayoutCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.setLayoutCount = _descriptorSetLayouts.size();
        createInfo.pSetLayouts = _descriptorSetLayouts.data();
        createInfo.pushConstantRangeCount = _ranges.size();
        createInfo.pPushConstantRanges = _ranges.data();

        VkPipelineLayout pipelineLayout;
        VKZ_CHECK_VULKAN(vkCreatePipelineLayout(device().logical, &createInfo, nullptr, &pipelineLayout));

        return pipelineLayout;
    }

    ComputePipelineLayoutBuilder &ComputePipelineLayoutBuilder::addPushConstantRange(VkPushConstantRange range) {
        _ranges.push_back(range);
        return *this;
    }

    ComputePipelineLayoutBuilder &ComputePipelineLayoutBuilder::clearRanges() {
        _ranges.clear();
        return *this;
    }

    ComputePipelineLayoutBuilder &ComputePipelineLayoutBuilder::clearLayouts() {
        _descriptorSetLayouts.clear();
        return *this;
    }

    ComputePipelineLayoutBuilder &ComputePipelineLayoutBuilder::clear() {
        _ranges.clear();
        _descriptorSetLayouts.clear();
        return *this;
    }


    void ComputePipelineLayoutBuilder::copy(const ComputePipelineLayoutBuilder &source) {
        _ranges = decltype(_ranges)(source._ranges.begin(), source._ranges.end());
        _descriptorSetLayouts = decltype(_descriptorSetLayouts)(source._descriptorSetLayouts.begin(),
                                                                source._descriptorSetLayouts.end());

    }

}