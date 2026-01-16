#pragma once

namespace vkz {

    class RasterizationStateBuilder : public GraphicsPipelineBuilder {
    public:
        RasterizationStateBuilder(Device device, GraphicsPipelineBuilder *parent);

        RasterizationStateBuilder &enableDepthClamp();

        RasterizationStateBuilder &disableDepthClamp();

        RasterizationStateBuilder &enableRasterizerDiscard();

        RasterizationStateBuilder &disableRasterizerDiscard();

        RasterizationStateBuilder &polygonModeFill();

        RasterizationStateBuilder &polygonModeLine();

        RasterizationStateBuilder &polygonModePoint();

        RasterizationStateBuilder &cullNone();

        RasterizationStateBuilder &cullFrontFace();

        RasterizationStateBuilder &cullBackFace();

        RasterizationStateBuilder &cullFrontAndBackFace();

        RasterizationStateBuilder &frontFaceCounterClockwise();

        RasterizationStateBuilder &frontFaceClockwise();

        RasterizationStateBuilder &enableDepthBias();

        RasterizationStateBuilder &disableDepthBias();

        RasterizationStateBuilder &depthBiasConstantFactor(float value);

        RasterizationStateBuilder &depthBiasClamp(float value);

        RasterizationStateBuilder &depthBiasSlopeFactor(float value);

        RasterizationStateBuilder &lineWidth(float value);

        VkPipelineRasterizationStateCreateInfo &buildRasterState();

        void copy(const RasterizationStateBuilder &source);

    private:
        VkPipelineRasterizationStateCreateInfo _info;
    };

}