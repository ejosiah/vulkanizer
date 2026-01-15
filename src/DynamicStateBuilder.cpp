#include "vulkanizer/DynamicStateBuilder.hpp"

namespace vkz {

    DynamicStateBuilder::DynamicStateBuilder(Device device, GraphicsPipelineBuilder *parent)
            : GraphicsPipelineBuilder(device, parent),
            _info{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO},
              _dynamicStates{} {}

    DynamicStateBuilder &DynamicStateBuilder::viewport() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::scissor() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::lineWidth() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::depthBias() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::blendConstants() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::depthBounds() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::stencilCompareMask() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::stencilWriteMask() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::stencilReferenceMask() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::cullMode() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_CULL_MODE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::frontFace() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_FRONT_FACE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::primitiveTopology() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::viewportWithCount() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::scissorWithCount() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::vertexInputBindingStride() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::depthTestEnable() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::depthWriteEnable() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::depthCompareOp() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::DepthBoundsTestEnable() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::stencilTestEnable() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::stencilOp() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_OP);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::rasterDiscardEnable() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::depthBiasEnable() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::primitiveRestartEnable() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::colorWriteEnable() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::polygonModeEnable() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::colorBlendEnable() {
        _dynamicStates.push_back(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT);
        return *this;
    }

    DynamicStateBuilder &DynamicStateBuilder::clear() {
        _dynamicStates.clear();
        return *this;
    }

    VkPipelineDynamicStateCreateInfo &DynamicStateBuilder::buildPipelineDynamicState() {
        _info.dynamicStateCount = VKZ_COUNT(_dynamicStates);
        _info.pDynamicStates = _dynamicStates.data();
        return _info;
    }

    void DynamicStateBuilder::copy(const DynamicStateBuilder &source) {
        _dynamicStates = decltype(_dynamicStates)(source._dynamicStates.begin(), source._dynamicStates.end());
    }

}
