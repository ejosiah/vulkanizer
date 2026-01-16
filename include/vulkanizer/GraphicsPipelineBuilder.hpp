#pragma once

#include "vkz.hpp"
#include "builder.hpp"

#include <string>
#include <memory>

namespace vkz {

    class GraphicsPipelineBuilder : public Builder {
    public:
        friend  class TessellationStateBuilder;
        explicit GraphicsPipelineBuilder(Device device);

        GraphicsPipelineBuilder(Device device, GraphicsPipelineBuilder* parent);

        GraphicsPipelineBuilder() = default;

        GraphicsPipelineBuilder(GraphicsPipelineBuilder&& source);

        virtual ~GraphicsPipelineBuilder();

        virtual ShaderStageBuilder& shaderStage();

        virtual VertexInputStateBuilder& vertexInputState();

        virtual InputAssemblyStateBuilder& inputAssemblyState();

        virtual TessellationStateBuilder& tessellationState();

        virtual ViewportStateBuilder& viewportState();

        virtual RasterizationStateBuilder& rasterizationState();

        virtual DepthStencilStateBuilder& depthStencilState();

        virtual ColorBlendStateBuilder& colorBlendState(void* next = nullptr);

        virtual MultisampleStateBuilder& multisampleState();

        virtual PipelineLayoutBuilder& layout();

        virtual DynamicStateBuilder& dynamicState();

        GraphicsPipelineBuilder& allowDerivatives();

        GraphicsPipelineBuilder& setDerivatives();

        GraphicsPipelineBuilder& subpass(uint32_t value);

        GraphicsPipelineBuilder& layout(VkPipelineLayout  aLayout);

        GraphicsPipelineBuilder& renderPass(VkRenderPass  aRenderPass);

        DynamicRenderPassBuilder& dynamicRenderPass();

        GraphicsPipelineBuilder& name(const std::string& value);

        GraphicsPipelineBuilder& reuse();

        GraphicsPipelineBuilder& basePipeline(VkPipeline& pipeline);

        GraphicsPipelineBuilder& pipelineCache(VkPipelineCache pCache);

        [[nodiscard]]
        GraphicsPipelineBuilder *parent() override;

        VkPipeline build();

        VkPipeline build(VkPipelineLayout& pipelineLayout);

        VkGraphicsPipelineCreateInfo createInfo();

        [[nodiscard]]
        GraphicsPipelineBuilder clone() const;

        void copy(const GraphicsPipelineBuilder& source);

        [[nodiscard]]
        VkPipelineLayout pipelineLayout() const {
            return _pipelineLayoutOwned;
        }

    protected:
        VkPipelineCreateFlags _flags = 0;
        VkRenderPass _renderPass{};
        VkPipelineLayout _pipelineLayout{};
        VkPipelineLayout _pipelineLayoutOwned{};
        uint32_t _subpass = 0;
        std::string _name;

        std::unique_ptr<ShaderStageBuilder> _shaderStageBuilder = nullptr;
        std::unique_ptr<VertexInputStateBuilder> _vertexInputStateBuilder = nullptr;
        std::unique_ptr<InputAssemblyStateBuilder> _inputAssemblyStateBuilder = nullptr;
        std::unique_ptr<PipelineLayoutBuilder> _pipelineLayoutBuilder = nullptr;
        std::unique_ptr<ViewportStateBuilder> _viewportStateBuilder = nullptr;
        std::unique_ptr<RasterizationStateBuilder> _rasterizationStateBuilder = nullptr;
        std::unique_ptr<MultisampleStateBuilder> _multisampleStateBuilder = nullptr;
        std::unique_ptr<DepthStencilStateBuilder> _depthStencilStateBuilder = nullptr;
        std::unique_ptr<ColorBlendStateBuilder> _colorBlendStateBuilder = nullptr ;
        std::unique_ptr<DynamicStateBuilder> _dynamicStateBuilder = nullptr;
        std::unique_ptr<TessellationStateBuilder> _tessellationStateBuilder = nullptr;
        std::unique_ptr<DynamicRenderPassBuilder> _dynamicRenderStateBuilder = nullptr;

        VkPipeline _basePipeline{};
        VkPipelineCache _pipelineCache{};
        void* nextChain = nullptr;

    };
}
#include "ShaderStageBuilder.hpp"
#include "VertexInputStateBuilder.hpp"
#include "InputAssemblyStateBuilder.hpp"
#include "PipelineLayoutBuilder.hpp"
#include "ViewportStateBuilder.hpp"
#include "RasterizationStateBuilder.hpp"
#include "MultisampleStateBuilder.hpp"
#include "DepthStencilStateBuilder.hpp"
#include "ColorBlendStateBuilder.hpp"
#include "DynamicStateBuilder.hpp"
#include "TessellationStateBuilder.hpp"
#include "DynamicRenderPassBuilder.hpp"
