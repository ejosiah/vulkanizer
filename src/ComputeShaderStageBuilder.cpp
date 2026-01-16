#include "vulkanizer/ComputePipelineBuilder.hpp"
#include "vulkanizer/creators.hpp"
#include "vulkanizer/util.hpp"
#include "vulkanizer/status.hpp"
#include "vulkanizer/detail/ComputeShaderStageBuilder.hpp"


vkz::ComputeShaderStageBuilder::ComputeShaderStageBuilder(vkz::Device device, vkz::ComputePipelineBuilder *parent)
    : ComputePipelineBuilder(device, parent) {}

vkz::ComputeShaderStageBuilder::ComputeShaderStageBuilder(vkz::ComputeShaderStageBuilder *parent)
: ComputePipelineBuilder(parent->_device, parent){}

vkz::ComputeShaderStageBuilder &
vkz::ComputeShaderStageBuilder::computeShader(const vkz::ComputeShaderStageBuilder::ShaderSource &source) {
    _shader.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    std::visit(overloaded{
            [&](const byte_string& source) { _shader.module = createShaderModule(_device, source); },
            [&](const std::vector<uint32_t>& source) { _shader.module = createShaderModule(_device, source); },
            [&](const std::string &source) { _shader.module = createShaderModule(_device, source); },
    }, source);

    return *this;
}

vkz::ComputeShaderStageBuilder::~ComputeShaderStageBuilder() {
    assert(_device.logical);
    if(_shader.module) {
        vkDestroyShaderModule(_device.logical, _shader.module, nullptr);
    }
}

void vkz::ComputeShaderStageBuilder::validate() const {
    if(_shader.stage != VK_SHADER_STAGE_COMPUTE_BIT || !_shader.module) {
        VKZ_THROW("shader module must be provided")
    }
}

VkPipelineShaderStageCreateInfo &vkz::ComputeShaderStageBuilder::buildShaderStage() {
    _createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    _createInfo.stage = _shader.stage;
    _createInfo.module = _shader.module;
    _createInfo.pName = _shader.entry;

    _specialization.mapEntryCount = _entries.size();
    _specialization.pMapEntries = _entries.data();
    _specialization.dataSize = _data.size();
    _specialization.pData = _data.data();
    _createInfo.pSpecializationInfo = &_specialization;

    return _createInfo;
}

void vkz::ComputeShaderStageBuilder::clearStages() {
    _shader.module = nullptr;
}
