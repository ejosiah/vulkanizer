#include "vulkanizer/graphics_pipeline_builder.hpp"

namespace vkz {

    input_assembly_state_builder::input_assembly_state_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent) {

    }

    input_assembly_state_builder &input_assembly_state_builder::enable_primitive_restart() {
        _primitiveRestartEnable = VK_TRUE;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::disable_primitive_restart() {
        _primitiveRestartEnable = VK_TRUE;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::points() {
        _topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::lines() {
        _topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::line_strip() {
        _topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::triangles() {
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::triangle_fan() {
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::triangle_strip() {
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::patches() {
        _topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::lines_with_adjacency() {
        _topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::line_strip_with_adjacency() {
        _topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::triangles_with_adjacency() {
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
        return *this;
    }

    input_assembly_state_builder &input_assembly_state_builder::triangle_strip_with_adjacency() {
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
        return *this;
    }

    VkPipelineInputAssemblyStateCreateInfo &input_assembly_state_builder::build_input_assembly_state() {
        if (_topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP || _topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) {
            _primitiveRestartEnable = VK_TRUE;
        } else {
            _primitiveRestartEnable = VK_FALSE;
        }
        _info.topology = _topology;
        _info.primitiveRestartEnable = _primitiveRestartEnable;

        return _info;
    }

    void input_assembly_state_builder::copy(const input_assembly_state_builder &source) {
        _topology = source._topology;
        _primitiveRestartEnable = source._primitiveRestartEnable;
    }
}