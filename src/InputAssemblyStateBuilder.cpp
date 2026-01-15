#include "vulkanizer/InputAssemblyStateBuilder.hpp"

namespace vkz {

    InputAssemblyStateBuilder::InputAssemblyStateBuilder(Device device, GraphicsPipelineBuilder *parent)
            : GraphicsPipelineBuilder(device, parent) {

    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::enablePrimitiveRestart() {
        _primitiveRestartEnable = VK_TRUE;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::disablePrimitiveRestart() {
        _primitiveRestartEnable = VK_TRUE;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::points() {
        _topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::lines() {
        _topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::lineStrip() {
        _topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::triangles() {
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::triangleFan() {
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::triangleStrip() {
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::patches() {
        _topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::linesWithAdjacency() {
        _topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::lineStripWithAdjacency() {
        _topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::trianglesWithAdjacency() {
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
        return *this;
    }

    InputAssemblyStateBuilder &InputAssemblyStateBuilder::triangleStripWithAdjacency() {
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
        return *this;
    }

    VkPipelineInputAssemblyStateCreateInfo &InputAssemblyStateBuilder::buildInputAssemblyState() {
        if (_topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP || _topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) {
            _primitiveRestartEnable = VK_TRUE;
        } else {
            _primitiveRestartEnable = VK_FALSE;
        }
        _info.topology = _topology;
        _info.primitiveRestartEnable = _primitiveRestartEnable;

        return _info;
    }

    void InputAssemblyStateBuilder::copy(const InputAssemblyStateBuilder &source) {
        _topology = source._topology;
        _primitiveRestartEnable = source._primitiveRestartEnable;
    }
}