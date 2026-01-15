#include "vulkanizer/PipelineLayoutBuilder.hpp"

namespace vkz {

    PipelineLayoutBuilder::PipelineLayoutBuilder(Device device, GraphicsPipelineBuilder *builder)
            : GraphicsPipelineBuilder(device, builder) {

    }

    PipelineLayoutBuilder &PipelineLayoutBuilder::addDescriptorSetLayout(VkDescriptorSetLayout layout) {
        _descriptorSetLayouts.push_back(layout);
        return *this;
    }

    PipelineLayoutBuilder &
    PipelineLayoutBuilder::addPushConstantRange(VkShaderStageFlags stage, uint32_t offset, uint32_t size) {
        _ranges.push_back({stage, offset, size});
        return *this;
    }

    VkPipelineLayout PipelineLayoutBuilder::buildPipelineLayout() const {

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

    PipelineLayoutBuilder &PipelineLayoutBuilder::addPushConstantRange(VkPushConstantRange range) {
        _ranges.push_back(range);
        return *this;
    }

    PipelineLayoutBuilder &PipelineLayoutBuilder::clearRanges() {
        _ranges.clear();
        return *this;
    }

    PipelineLayoutBuilder &PipelineLayoutBuilder::clearLayouts() {
        _descriptorSetLayouts.clear();
        return *this;
    }

    PipelineLayoutBuilder &PipelineLayoutBuilder::clear() {
        _ranges.clear();
        _descriptorSetLayouts.clear();
        return *this;
    }


    void PipelineLayoutBuilder::copy(const PipelineLayoutBuilder &source) {
        _ranges = decltype(_ranges)(source._ranges.begin(), source._ranges.end());
        _descriptorSetLayouts = decltype(_descriptorSetLayouts)(source._descriptorSetLayouts.begin(),
                                                                source._descriptorSetLayouts.end());

    }

}