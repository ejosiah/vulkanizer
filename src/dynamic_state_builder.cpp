#include "vulkanizer/graphics_pipeline_builder.hpp"

namespace vkz {

    dynamic_state_builder::dynamic_state_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent),
            _info{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO},
              _dynamic_states{} {}

    dynamic_state_builder &dynamic_state_builder::viewport() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::scissor() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_SCISSOR);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::line_width() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::depth_bias() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::blend_constants() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::depth_bounds() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::stencil_compare_mask() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::stencil_write_mask() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::stencil_reference_mask() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::cull_mode() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_CULL_MODE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::front_face() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_FRONT_FACE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::primitive_topology() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::viewport_with_count() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::scissor_with_count() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::vertex_input_binding_stride() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::depth_test_enable() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::depth_write_enable() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::depth_compare_op() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::depth_bounds_test_enable() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::stencil_test_enable() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::stencil_op() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_STENCIL_OP);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::raster_discard_enable() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::depth_bias_enable() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::primitive_restart_enable() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::color_write_enable() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::polygon_mode_enable() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::color_blend_enable() {
        _dynamic_states.push_back(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT);
        return *this;
    }

    dynamic_state_builder &dynamic_state_builder::clear() {
        _dynamic_states.clear();
        return *this;
    }

    VkPipelineDynamicStateCreateInfo &dynamic_state_builder::build_pipeline_dynamic_state() {
        _info.dynamicStateCount = VKZ_COUNT(_dynamic_states);
        _info.pDynamicStates = _dynamic_states.data();
        return _info;
    }

    void dynamic_state_builder::copy(const dynamic_state_builder &source) {
        _dynamic_states = decltype(_dynamic_states)(source._dynamic_states.begin(), source._dynamic_states.end());
    }

}
