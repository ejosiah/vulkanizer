#include "vulkanizer/graphics_pipeline_builder.hpp"

namespace vkz {

    depth_stencil_state_builder::depth_stencil_state_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent),
            _info{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO },
            _front{new stencil_op_state_builder{this}},
            _back{new stencil_op_state_builder{this}}
    {
        dynamic_cast<depth_stencil_state_builder *>(_front)->_front = _front;
        dynamic_cast<depth_stencil_state_builder *>(_front)->_back = _back;
        dynamic_cast<depth_stencil_state_builder *>(_back)->_front = _front;
        dynamic_cast<depth_stencil_state_builder *>(_back)->_back = _back;
    }

    depth_stencil_state_builder::depth_stencil_state_builder(depth_stencil_state_builder *parent)
            : graphics_pipeline_builder(parent->_device, parent) {
        _info.depthTestEnable = VK_FALSE;
        _info.depthWriteEnable = VK_FALSE;
        _info.minDepthBounds = 0.f;
        _info.maxDepthBounds = 1.f;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::enable_depth_test() {
        _info.depthTestEnable = VK_TRUE;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::disable_depth_test() {
        _info.depthTestEnable = VK_FALSE;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::enable_depth_write() {
        _info.depthWriteEnable = VK_TRUE;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::disable_depth_write() {
        _info.depthWriteEnable = VK_FALSE;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::compare_op_never() {
        _info.depthCompareOp = VK_COMPARE_OP_NEVER;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::compare_op_less() {
        _info.depthCompareOp = VK_COMPARE_OP_LESS;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::compare_op_equal() {
        _info.depthCompareOp = VK_COMPARE_OP_EQUAL;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::compare_op_less_or_equal() {
        _info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::compare_op_greater() {
        _info.depthCompareOp = VK_COMPARE_OP_GREATER;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::compare_op_greater_or_equal() {
        _info.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::compare_op_not_equal() {
        _info.depthCompareOp = VK_COMPARE_OP_NOT_EQUAL;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::compare_op_always() {
        _info.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::enable_depth_bounds_test() {
        _info.depthBoundsTestEnable = VK_TRUE;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::disable_depth_bounds_test() {
        _info.depthBoundsTestEnable = VK_FALSE;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::enable_stencil_test() {
        _info.stencilTestEnable = VK_TRUE;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::disable_stencil_test() {
        _info.stencilTestEnable = VK_FALSE;
        return *this;
    }

    stencil_op_state_builder &depth_stencil_state_builder::stencil_op_front() {
        return *_front;
    }

    stencil_op_state_builder &depth_stencil_state_builder::stencil_op_back() {
        return *_back;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::min_depth_bounds(float value) {
        _info.minDepthBounds = value;
        return *this;
    }

    depth_stencil_state_builder &depth_stencil_state_builder::max_depth_bounds(float value) {
        _info.maxDepthBounds = value;
        return *this;
    }

    VkPipelineDepthStencilStateCreateInfo &depth_stencil_state_builder::build_depth_stencil_state() {
        _info.front = _front->build_stencil_op_state();
        _info.back = _back->build_stencil_op_state();
        return _info;
    }

    void depth_stencil_state_builder::copy(const depth_stencil_state_builder &source) {
        _front->_stencil_op_state = source._front->_stencil_op_state;
        _back->_stencil_op_state = source._front->_stencil_op_state;
        _info = source._info;
    }


    stencil_op_state_builder::stencil_op_state_builder(depth_stencil_state_builder *parent)
            : depth_stencil_state_builder(parent), _stencil_op_state{} {
    }

    stencil_op_state_builder &stencil_op_state_builder::fail_opKeep() {
        _stencil_op_state.failOp = VK_STENCIL_OP_KEEP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::fail_opZero() {
        _stencil_op_state.failOp = VK_STENCIL_OP_ZERO;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::fail_opReplace() {
        _stencil_op_state.failOp = VK_STENCIL_OP_REPLACE;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::fail_opIncrementAndClamp() {
        _stencil_op_state.failOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::fail_opDecrementAndClamp() {
        _stencil_op_state.failOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::fail_opInvert() {
        _stencil_op_state.failOp = VK_STENCIL_OP_INVERT;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::fail_opIncrementAndWrap() {
        _stencil_op_state.failOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::fail_opDecrementAndWrap() {
        _stencil_op_state.failOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::pass_opKeep() {
        _stencil_op_state.passOp = VK_STENCIL_OP_KEEP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::pass_opZero() {
        _stencil_op_state.passOp = VK_STENCIL_OP_ZERO;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::pass_opReplace() {
        _stencil_op_state.passOp = VK_STENCIL_OP_REPLACE;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::pass_opIncrementAndClamp() {
        _stencil_op_state.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::pass_opDecrementAndClamp() {
        _stencil_op_state.passOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::pass_opInvert() {
        _stencil_op_state.passOp = VK_STENCIL_OP_INVERT;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::depth_fail_opKeep() {
        _stencil_op_state.depthFailOp = VK_STENCIL_OP_KEEP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::depth_fail_opZero() {
        _stencil_op_state.depthFailOp = VK_STENCIL_OP_ZERO;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::depth_fail_opReplace() {
        _stencil_op_state.depthFailOp = VK_STENCIL_OP_REPLACE;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::depth_fail_opIncrementAndClamp() {
        _stencil_op_state.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::depth_fail_opDecrementAndClamp() {
        _stencil_op_state.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::depth_fail_opInvert() {
        _stencil_op_state.depthFailOp = VK_STENCIL_OP_INVERT;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::depth_fail_opIncrementAndWrap() {
        _stencil_op_state.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::depth_fail_opDecrementAndWrap() {
        _stencil_op_state.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::pass_opIncrementAndWrap() {
        _stencil_op_state.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::pass_opDecrementAndWrap() {
        _stencil_op_state.passOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::compare_op_never() {
        _stencil_op_state.compareOp = VK_COMPARE_OP_NEVER;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::compare_op_less() {
        _stencil_op_state.compareOp = VK_COMPARE_OP_LESS;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::compare_op_equal() {
        _stencil_op_state.compareOp = VK_COMPARE_OP_EQUAL;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::compare_op_less_or_equal() {
        _stencil_op_state.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::compare_op_greater() {
        _stencil_op_state.compareOp = VK_COMPARE_OP_GREATER;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::compare_op_greater_or_equal() {
        _stencil_op_state.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::compare_op_not_equal() {
        _stencil_op_state.compareOp = VK_COMPARE_OP_NOT_EQUAL;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::compare_op_always() {
        _stencil_op_state.compareOp = VK_COMPARE_OP_ALWAYS;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::compare_mask(uint32_t value) {
        _stencil_op_state.compareMask = value;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::write_mask(uint32_t value) {
        _stencil_op_state.writeMask = value;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::reference(uint32_t value) {
        _stencil_op_state.reference = value;
        return *this;
    }

    stencil_op_state_builder &stencil_op_state_builder::clear_stencil_state() {
        _stencil_op_state = {};
        return *this;
    }

    VkStencilOpState stencil_op_state_builder::build_stencil_op_state() {
        return _stencil_op_state;
    }
}