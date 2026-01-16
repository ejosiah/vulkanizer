#include "vulkanizer/ComputePipelineBuilder.hpp"
#include "vulkanizer/creators.hpp"
#include "vulkanizer/util.hpp"
#include "vulkanizer/status.hpp"

vkz::ComputeShaderStageBuilder::ComputeShaderStageBuilder(vkz::Device device, vkz::ComputePipelineBuilder *parent)
    : ComputePipelineBuilder(device, parent) {}

vkz::ComputeShaderStageBuilder::ComputeShaderStageBuilder(vkz::ComputeShaderStageBuilder *parent)
: ComputePipelineBuilder(parent->_device, parent){}

vkz::ComputeShaderStageBuilder &
vkz::ComputeShaderStageBuilder::computeShader(const vkz::ComputeShaderStageBuilder::ShaderSource &source) {
    _stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    std::visit(overloaded{
            [&](const byte_string source) { _stage.module = createShaderModule(_device, source); },
            [&](const std::vector<uint32_t> source) { _stage.module = createShaderModule(_device, source); },
            [&](const std::string &source) { _stage.module = createShaderModule(_device, source); },
    }, source);

    return *this;
}

void vkz::ComputeShaderStageBuilder::validate() const {
    if(_stage.stage != VK_SHADER_STAGE_COMPUTE_BIT || !_stage.module) {
        VKZ_THROW("shader module must be provided")
    }
}

VkPipelineShaderStageCreateInfo &vkz::ComputeShaderStageBuilder::buildShaderStage() {
    _createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    _createInfo.stage = _stage.stage;
    _createInfo.module = _stage.module;
    _createInfo.pName = _stage.entry;

    _specialization.mapEntryCount = _entries.size();
    _specialization.pMapEntries = _entries.data();
    _specialization.dataSize = _data.size();
    _specialization.pData = _data.data();
    _createInfo.pSpecializationInfo = &_specialization;

    return _createInfo;
}

void vkz::ComputeShaderStageBuilder::clearStages() {
    _stage.module = nullptr;
}
