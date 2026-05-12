#include "vulkanizer/graphics_pipeline_builder.hpp"

namespace vkz {

    rasterization_state_builder::rasterization_state_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent),
            _info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .lineWidth = 1.0f
            }
    {}

    rasterization_state_builder &rasterization_state_builder::enable_depth_clamp() {
        _info.depthClampEnable = VK_TRUE;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::disable_depth_clamp() {
        _info.depthClampEnable = VK_FALSE;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::enable_rasterizer_discard() {
        _info.rasterizerDiscardEnable = VK_TRUE;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::disable_rasterizer_discard() {
        _info.rasterizerDiscardEnable = VK_FALSE;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::polygon_mode_fill() {
        _info.polygonMode = VK_POLYGON_MODE_FILL;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::polygon_mode_line() {
        _info.polygonMode = VK_POLYGON_MODE_LINE;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::polygon_mode_point() {
        _info.polygonMode = VK_POLYGON_MODE_POINT;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::cull_none() {
        _info.cullMode = VK_CULL_MODE_NONE;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::cull_front_face() {
        _info.cullMode = VK_CULL_MODE_FRONT_BIT;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::cull_back_face() {
        _info.cullMode = VK_CULL_MODE_BACK_BIT;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::cull_front_and_back_face() {
        _info.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::front_face_counter_clockwise() {
        _info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::front_face_clockwise() {
        _info.frontFace = VK_FRONT_FACE_CLOCKWISE;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::enable_depth_bias() {
        _info.depthBiasEnable = VK_TRUE;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::disable_depth_bias() {
        _info.depthBiasEnable = VK_FALSE;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::depth_bias_constant_factor(float value) {
        _info.depthBiasConstantFactor = value;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::depth_bias_clamp(float value) {
        _info.depthBiasClamp = value;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::depth_bias_slope_factor(float value) {
        _info.depthBiasSlopeFactor = value;
        return *this;
    }

    rasterization_state_builder &rasterization_state_builder::line_width(float value) {
        _info.lineWidth = value;
        return *this;
    }

    VkPipelineRasterizationStateCreateInfo &rasterization_state_builder::build_raster_state() {
        return _info;
    }

    void rasterization_state_builder::copy(const rasterization_state_builder &source) {
        _info = source._info;
    }

}