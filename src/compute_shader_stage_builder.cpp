#include "vulkanizer/compute_pipeline_builder.hpp"
#include "vulkanizer/creators.hpp"
#include "vulkanizer/util.hpp"
#include "vulkanizer/status.hpp"
#include "vulkanizer/detail/compute_shader_stage_builder.hpp"


vkz::compute_shader_stage_builder::compute_shader_stage_builder(vkz::device device, vkz::compute_pipeline_builder *parent)
    : compute_pipeline_builder(device, parent) {}

vkz::compute_shader_stage_builder::compute_shader_stage_builder(vkz::compute_shader_stage_builder *parent)
: compute_pipeline_builder(parent->_device, parent){}

vkz::compute_shader_stage_builder &
vkz::compute_shader_stage_builder::compute_shader(const vkz::compute_shader_stage_builder::shader_source &source) {
    _shader.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    std::visit(overloaded{
            [&](const byte_string& source) { _shader.module = create_shader_module(_device, source); },
            [&](const std::vector<uint32_t>& source) { _shader.module = create_shader_module(_device, source); },
            [&](const std::string &source) { _shader.module = create_shader_module(_device, source); },
    }, source);

    return *this;
}

vkz::compute_shader_stage_builder::~compute_shader_stage_builder() {
    assert(_device.logical);
    if(_shader.module) {
        vkDestroyShaderModule(_device.logical, _shader.module, nullptr);
    }
}

void vkz::compute_shader_stage_builder::validate() const {
    if(_shader.stage != VK_SHADER_STAGE_COMPUTE_BIT || !_shader.module) {
        VKZ_THROW("shader module must be provided")
    }
}

VkPipelineShaderStageCreateInfo &vkz::compute_shader_stage_builder::build_shader_stage() {
    _create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    _create_info.stage = _shader.stage;
    _create_info.module = _shader.module;
    _create_info.pName = _shader.entry;

    _specialization.mapEntryCount = _entries.size();
    _specialization.pMapEntries = _entries.data();
    _specialization.dataSize = _data.size();
    _specialization.pData = _data.data();
    _create_info.pSpecializationInfo = &_specialization;

    return _create_info;
}

void vkz::compute_shader_stage_builder::clear_stages() {
    _shader.module = nullptr;
}
