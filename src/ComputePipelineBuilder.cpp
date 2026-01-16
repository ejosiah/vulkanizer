#include "vulkanizer/ComputePipelineBuilder.hpp"
#include "vulkanizer/struct_mapping.hpp"

vkz::ComputePipelineBuilder::ComputePipelineBuilder(vkz::Device device)
: Builder{device, nullptr}
, _shaderStageBuilder{std::make_unique<ComputeShaderStageBuilder>(device, this)}
, _pipelineLayoutBuilder{std::make_unique<ComputePipelineLayoutBuilder>(device, this)}
{}

vkz::ComputePipelineBuilder::ComputePipelineBuilder(vkz::Device device, vkz::ComputePipelineBuilder *parent)
:Builder(device, parent){}

vkz::ComputePipelineBuilder::ComputePipelineBuilder(vkz::ComputePipelineBuilder &&source) noexcept {
    _shaderStageBuilder = std::move(source._shaderStageBuilder);
    _pipelineLayoutBuilder = std::move(source._pipelineLayoutBuilder);
    _name = std::move(source._name);
    _flags = source._flags;
    _pipelineLayout = std::exchange(source._pipelineLayout, nullptr);
    _pipelineLayoutOwned = std::exchange(source._pipelineLayoutOwned, nullptr);
    _basePipeline = std::exchange(source._basePipeline, nullptr);
    _pipelineCache = std::exchange(source._pipelineCache, nullptr);
    _nextChain = std::exchange(source._nextChain, nullptr);
    _parent = std::exchange(source._parent, nullptr);
    _device = source._device;
}

vkz::ComputePipelineBuilder *vkz::ComputePipelineBuilder::parent() {
    return dynamic_cast<ComputePipelineBuilder *>(Builder::parent());
}

vkz::ComputeShaderStageBuilder &vkz::ComputePipelineBuilder::shaderStage() {
    if (parent()) {
        return parent()->shaderStage();
    }
    return *_shaderStageBuilder;
}

vkz::ComputePipelineLayoutBuilder &vkz::ComputePipelineBuilder::layout(VkPipelineLayout aLayout) {
    if (parent()) {
        return parent()->layout(aLayout);
    }
    return *_pipelineLayoutBuilder;
}

VkPipeline vkz::ComputePipelineBuilder::build() {
    if (parent()) {
        return parent()->build();
    }
    if (!_pipelineLayout) {
        throw std::runtime_error{"either provide or create a pipelineLayout"};
    }
    VkPipelineLayout unused{};
    return build(unused);
}

VkPipeline vkz::ComputePipelineBuilder::build(VkPipelineLayout &pipelineLayout) {
    if (parent()) {
        return parent()->build(pipelineLayout);
    }
    auto info = createInfo();
    pipelineLayout = std::move(_pipelineLayoutOwned);

    VkPipeline pipeline;
    VKZ_CHECK_VULKAN(vkCreateComputePipelines(device().logical, nullptr, 1, &info, nullptr, &pipeline));
    if (!_name.empty()) {
        setName<VK_OBJECT_TYPE_PIPELINE>(device(), _name, pipeline);
        setName<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(device(), _name, pipelineLayout);
    }
    _shaderStageBuilder->clearStages();
    return pipeline;
}

VkComputePipelineCreateInfo vkz::ComputePipelineBuilder::createInfo() {
    if (parent()) return parent()->createInfo();

    auto &shaderStage = _shaderStageBuilder->buildShaderStage();

    auto info = makeStruct<VkComputePipelineCreateInfo>();
    info.flags = _flags;
    info.stage = shaderStage;

    if (_flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT) {
        assert(_basePipeline);
        info.basePipelineHandle = _basePipeline;
        info.basePipelineIndex = -1;
    }

    if (!_pipelineLayout) {
        _pipelineLayoutOwned = _pipelineLayoutBuilder->buildPipelineLayout();
        info.layout = _pipelineLayoutOwned;
    } else {
        info.layout = _pipelineLayout;
    }

    return info;
}
