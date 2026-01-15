#include "vulkanizer/GraphicsPipelineBuilder.hpp"
#include "vulkanizer/struct_mapping.hpp"

namespace vkz {

    GraphicsPipelineBuilder::GraphicsPipelineBuilder(Device device)
            : Builder{device, nullptr}, _shaderStageBuilder{std::make_unique<ShaderStageBuilder>(device, this)},
              _vertexInputStateBuilder{std::make_unique<VertexInputStateBuilder>(device, this)},
              _inputAssemblyStateBuilder{std::make_unique<InputAssemblyStateBuilder>(device, this)},
              _pipelineLayoutBuilder{std::make_unique<PipelineLayoutBuilder>(device, this)},
              _viewportStateBuilder{std::make_unique<ViewportStateBuilder>(device, this)},
              _rasterizationStateBuilder{std::make_unique<RasterizationStateBuilder>(device, this)},
              _multisampleStateBuilder{std::make_unique<MultisampleStateBuilder>(device, this)},
              _depthStencilStateBuilder{std::make_unique<DepthStencilStateBuilder>(device, this)},
              _colorBlendStateBuilder{std::make_unique<ColorBlendStateBuilder>(device, this)},
              _dynamicStateBuilder{std::make_unique<DynamicStateBuilder>(device, this)},
              _tessellationStateBuilder{std::make_unique<TessellationStateBuilder>(device, this)},
              _dynamicRenderStateBuilder{std::make_unique<DynamicRenderPassBuilder>(device, this)} {}

    GraphicsPipelineBuilder::GraphicsPipelineBuilder(Device device, GraphicsPipelineBuilder *parent)
            : Builder{device, parent} {
    }

    GraphicsPipelineBuilder::GraphicsPipelineBuilder(GraphicsPipelineBuilder &&source) {
        _shaderStageBuilder = std::move(source._shaderStageBuilder);
        _vertexInputStateBuilder = std::move(source._vertexInputStateBuilder);
        _inputAssemblyStateBuilder = std::move(source._inputAssemblyStateBuilder);
        _pipelineLayoutBuilder = std::move(source._pipelineLayoutBuilder);
        _viewportStateBuilder = std::move(source._viewportStateBuilder);
        _rasterizationStateBuilder = std::move(source._rasterizationStateBuilder);
        _multisampleStateBuilder = std::move(source._multisampleStateBuilder);
        _depthStencilStateBuilder = std::move(source._depthStencilStateBuilder);
        _colorBlendStateBuilder = std::move(source._colorBlendStateBuilder);
        _dynamicStateBuilder = std::move(source._dynamicStateBuilder);
        _tessellationStateBuilder = std::move(source._tessellationStateBuilder);
        _name = std::move(source._name);
        _flags = source._flags;
        _renderPass = std::exchange(source._renderPass, VK_NULL_HANDLE);
        _pipelineLayout = std::move(source._pipelineLayout);
        _pipelineLayoutOwned = std::move(source._pipelineLayoutOwned);
        _subpass = source._subpass;
        _basePipeline = std::move(source._basePipeline);
        _pipelineCache = std::move(source._pipelineCache);
        nextChain = std::exchange(source.nextChain, nullptr);
        _parent = std::exchange(source._parent, nullptr);
        _device = source._device;
    }


    GraphicsPipelineBuilder::~GraphicsPipelineBuilder() = default;

    ShaderStageBuilder &GraphicsPipelineBuilder::shaderStage() {
        if (parent()) {
            return parent()->shaderStage();
        }
        return *_shaderStageBuilder;
    }

    VertexInputStateBuilder &GraphicsPipelineBuilder::vertexInputState() {
        if (parent()) {
            return parent()->vertexInputState();
        }
        return *_vertexInputStateBuilder;
    }

    GraphicsPipelineBuilder *GraphicsPipelineBuilder::parent() {
        return dynamic_cast<GraphicsPipelineBuilder *>(Builder::parent());
    }

    InputAssemblyStateBuilder &GraphicsPipelineBuilder::inputAssemblyState() {
        if (parent()) {
            return parent()->inputAssemblyState();
        }
        return *_inputAssemblyStateBuilder;
    }

    TessellationStateBuilder &GraphicsPipelineBuilder::tessellationState() {
        if (parent()) {
            return parent()->tessellationState();
        }
        return *_tessellationStateBuilder;
    }

    GraphicsPipelineBuilder &GraphicsPipelineBuilder::allowDerivatives() {
        if (parent()) {
            return parent()->allowDerivatives();
        }
        _flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
        return *this;
    }

    GraphicsPipelineBuilder &GraphicsPipelineBuilder::setDerivatives() {
        if (parent()) {
            return parent()->setDerivatives();
        }
        _flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        return *this;
    }

    GraphicsPipelineBuilder &GraphicsPipelineBuilder::subpass(uint32_t value) {
        if (parent()) {
            return parent()->subpass(value);
        }
        _subpass = value;
        return *this;
    }


    GraphicsPipelineBuilder &GraphicsPipelineBuilder::layout(VkPipelineLayout aLayout) {
        if (parent()) {
            return parent()->layout(aLayout);
        }
        _pipelineLayout = aLayout;
        return *this;
    }

    GraphicsPipelineBuilder &GraphicsPipelineBuilder::renderPass(VkRenderPass aRenderPass) {
        if (parent()) {
            return parent()->renderPass(aRenderPass);
        }
        _renderPass = aRenderPass;
        return *this;
    }

    DynamicRenderPassBuilder &GraphicsPipelineBuilder::dynamicRenderPass() {
        if (parent()) {
            return parent()->dynamicRenderPass();
        }
        _renderPass = VK_NULL_HANDLE;
        _dynamicRenderStateBuilder->enable();
        return *_dynamicRenderStateBuilder;
    }

    PipelineLayoutBuilder &GraphicsPipelineBuilder::layout() {
        if (parent()) {
            return parent()->layout();
        }
        return *_pipelineLayoutBuilder;
    }

    VkPipeline GraphicsPipelineBuilder::build() {
        if (parent()) {
            return parent()->build();
        }
        if (!_pipelineLayout) {
            throw std::runtime_error{"either provide or create a pipelineLayout"};
        }
        VkPipelineLayout unused{};
        return build(unused);
    }


    VkPipeline GraphicsPipelineBuilder::build(VkPipelineLayout &pipelineLayout) {
        if (parent()) {
            return parent()->build(pipelineLayout);
        }
        auto info = createInfo();
        pipelineLayout = std::move(_pipelineLayoutOwned);

        VkPipeline pipeline;
        VKZ_CHECK_VULKAN(vkCreateGraphicsPipelines(device().logical, nullptr, 1, &info, nullptr, &pipeline));
        if (!_name.empty()) {
           setName<VK_OBJECT_TYPE_PIPELINE>(device(), _name, pipeline);
           setName<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(device(), _name, pipelineLayout);
        }
        _shaderStageBuilder->clearStages();
        return pipeline;
    }

    VkGraphicsPipelineCreateInfo GraphicsPipelineBuilder::createInfo() {
        if (parent()) return parent()->createInfo();

        auto &shaderStages = _shaderStageBuilder->buildShaderStage();
        auto &vertexInputState = _vertexInputStateBuilder->buildVertexInputState();
        auto &inputAssemblyState = _inputAssemblyStateBuilder->buildInputAssemblyState();
        auto &viewportState = _viewportStateBuilder->buildViewportState();
        auto &rasterState = _rasterizationStateBuilder->buildRasterState();
        auto &multisampleState = _multisampleStateBuilder->buildMultisampleState();
        auto &depthStencilState = _depthStencilStateBuilder->buildDepthStencilState();
        auto &colorBlendState = _colorBlendStateBuilder->buildColorBlendState();
        auto &dynamicState = _dynamicStateBuilder->buildPipelineDynamicState();
        auto &tessellationState = _tessellationStateBuilder->buildTessellationState();

        auto info = makeStruct<VkGraphicsPipelineCreateInfo>();
        info.flags = _flags;
        info.stageCount = VKZ_COUNT(shaderStages);
        info.pStages = shaderStages.data();
        info.pVertexInputState = &vertexInputState;
        info.pInputAssemblyState = &inputAssemblyState;
        info.pTessellationState = &tessellationState;
        info.pViewportState = &viewportState;
        info.pRasterizationState = &rasterState;
        info.pMultisampleState = &multisampleState;
        info.pDepthStencilState = &depthStencilState;
        info.pColorBlendState = &colorBlendState;
        info.pDynamicState = &dynamicState;

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
        info.renderPass = _renderPass;
        info.subpass = _subpass;

        if (_dynamicRenderStateBuilder->enabled()) {
            info.pNext = &_dynamicRenderStateBuilder->buildDynamicRenderInfo();
        }

        return info;
    }

    ViewportStateBuilder &GraphicsPipelineBuilder::viewportState() {
        if (parent()) {
            return parent()->viewportState();
        }
        return *_viewportStateBuilder;
    }

    RasterizationStateBuilder &GraphicsPipelineBuilder::rasterizationState() {
        if (parent()) {
            return parent()->rasterizationState();
        }
        return *_rasterizationStateBuilder;
    }

    DepthStencilStateBuilder &GraphicsPipelineBuilder::depthStencilState() {
        if (parent()) {
            return parent()->depthStencilState();
        }
        return *_depthStencilStateBuilder;
    }

    ColorBlendStateBuilder &GraphicsPipelineBuilder::colorBlendState(void *next) {
        if (parent()) {
            return parent()->colorBlendState(next);
        }
        _colorBlendStateBuilder->nextChain = next;
        return *_colorBlendStateBuilder;
    }

    GraphicsPipelineBuilder &GraphicsPipelineBuilder::name(const std::string &value) {
        if (parent()) {
            parent()->name(value);
        }
        _name = value;
        return *this;
    }

    GraphicsPipelineBuilder &GraphicsPipelineBuilder::reuse() {
        if (parent()) {
            parent()->reuse();
        }
        _vertexInputStateBuilder->clear();
        _shaderStageBuilder->clear();
        _pipelineLayoutBuilder->clearLayouts();
        _pipelineLayoutBuilder->clearRanges();
        return *this;
    }

    GraphicsPipelineBuilder &GraphicsPipelineBuilder::basePipeline(VkPipeline &pipeline) {
        setDerivatives();
        if (parent()) {
            parent()->basePipeline(pipeline);
        }
        _basePipeline = pipeline;
        return *this;
    }

    MultisampleStateBuilder &GraphicsPipelineBuilder::multisampleState() {
        if (parent()) {
            return parent()->multisampleState();
        }
        return *_multisampleStateBuilder;
    }

    GraphicsPipelineBuilder &GraphicsPipelineBuilder::pipelineCache(VkPipelineCache pCache) {
        if (parent()) {
            parent()->pipelineCache(pCache);
        }
        _pipelineCache = pCache;
        return *this;
    }

    GraphicsPipelineBuilder GraphicsPipelineBuilder::clone() const {
        GraphicsPipelineBuilder aClone{_device};
        aClone.copy(*this);

        return aClone;
    }

    DynamicStateBuilder &GraphicsPipelineBuilder::dynamicState() {
        if (parent()) {
            return parent()->dynamicState();
        }
        return *_dynamicStateBuilder;
    }


    void GraphicsPipelineBuilder::copy(const GraphicsPipelineBuilder &source) {
        _flags = source._flags;
        _renderPass = source._renderPass;
        _pipelineLayout = source._pipelineLayout;
        _pipelineLayoutOwned = source._pipelineLayoutOwned;
        _subpass = source._subpass;
        _name = source._name;

        _shaderStageBuilder->copy(*source._shaderStageBuilder);
        _vertexInputStateBuilder->copy(*source._vertexInputStateBuilder);
        _inputAssemblyStateBuilder->copy(*source._inputAssemblyStateBuilder);
        _pipelineLayoutBuilder->copy(*source._pipelineLayoutBuilder);
        _viewportStateBuilder->copy(*source._viewportStateBuilder);
        _rasterizationStateBuilder->copy(*source._rasterizationStateBuilder);
        _multisampleStateBuilder->copy(*source._multisampleStateBuilder);
        _depthStencilStateBuilder->copy(*source._depthStencilStateBuilder);
        _colorBlendStateBuilder->copy(*source._colorBlendStateBuilder);
        _dynamicStateBuilder->copy(*source._dynamicStateBuilder);
        _tessellationStateBuilder->copy(*source._tessellationStateBuilder);

        _basePipeline = source._basePipeline;
        _pipelineCache = source._pipelineCache;
        nextChain = source.nextChain;
    }

}