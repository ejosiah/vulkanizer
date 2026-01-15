#pragma once

#include "GraphicsPipelineBuilder.hpp"

namespace vkz {

    class InputAssemblyStateBuilder : public GraphicsPipelineBuilder {
    public:
        InputAssemblyStateBuilder(Device device, GraphicsPipelineBuilder *parent);

        InputAssemblyStateBuilder &enablePrimitiveRestart();

        InputAssemblyStateBuilder &disablePrimitiveRestart();

        InputAssemblyStateBuilder &points();

        InputAssemblyStateBuilder &lines();

        InputAssemblyStateBuilder &lineStrip();

        InputAssemblyStateBuilder &triangles();

        InputAssemblyStateBuilder &triangleFan();

        InputAssemblyStateBuilder &triangleStrip();

        InputAssemblyStateBuilder &patches();

        InputAssemblyStateBuilder &linesWithAdjacency();

        InputAssemblyStateBuilder &lineStripWithAdjacency();

        InputAssemblyStateBuilder &trianglesWithAdjacency();

        InputAssemblyStateBuilder &triangleStripWithAdjacency();

        VkPipelineInputAssemblyStateCreateInfo &buildInputAssemblyState();

        void copy(const InputAssemblyStateBuilder &source);

    private:
        VkPrimitiveTopology _topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        VkBool32 _primitiveRestartEnable{VK_FALSE};
        VkPipelineInputAssemblyStateCreateInfo _info{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

    };
}