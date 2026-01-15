#pragma once

#include "GraphicsPipelineBuilder.hpp"

namespace vkz {

    class DynamicStateBuilder : public GraphicsPipelineBuilder {
    public:
        DynamicStateBuilder(Device device, GraphicsPipelineBuilder *parent);

        DynamicStateBuilder &viewport();

        DynamicStateBuilder &scissor();

        DynamicStateBuilder &lineWidth();

        DynamicStateBuilder &depthBias();

        DynamicStateBuilder &blendConstants();

        DynamicStateBuilder &depthBounds();

        DynamicStateBuilder &stencilCompareMask();

        DynamicStateBuilder &stencilWriteMask();

        DynamicStateBuilder &stencilReferenceMask();

        DynamicStateBuilder &cullMode();

        DynamicStateBuilder &frontFace();

        DynamicStateBuilder &primitiveTopology();

        DynamicStateBuilder &viewportWithCount();

        DynamicStateBuilder &scissorWithCount();

        DynamicStateBuilder &vertexInputBindingStride();

        DynamicStateBuilder &depthTestEnable();

        DynamicStateBuilder &depthWriteEnable();

        DynamicStateBuilder &depthCompareOp();

        DynamicStateBuilder &DepthBoundsTestEnable();

        DynamicStateBuilder &stencilTestEnable();

        DynamicStateBuilder &stencilOp();

        DynamicStateBuilder &rasterDiscardEnable();

        DynamicStateBuilder &depthBiasEnable();

        DynamicStateBuilder &primitiveRestartEnable();

        DynamicStateBuilder &colorWriteEnable();

        DynamicStateBuilder &polygonModeEnable();

        DynamicStateBuilder &colorBlendEnable();

        DynamicStateBuilder &clear();

        VkPipelineDynamicStateCreateInfo &buildPipelineDynamicState();

        void copy(const DynamicStateBuilder &source);

    private:
        std::vector<VkDynamicState> _dynamicStates;
        VkPipelineDynamicStateCreateInfo _info;
    };
}