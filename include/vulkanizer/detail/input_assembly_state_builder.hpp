#pragma once

namespace vkz {

    class input_assembly_state_builder : public graphics_pipeline_builder {
    public:
        input_assembly_state_builder(vkz::device device, graphics_pipeline_builder *parent);

        input_assembly_state_builder &enable_primitive_restart();

        input_assembly_state_builder &disable_primitive_restart();

        input_assembly_state_builder &points();

        input_assembly_state_builder &lines();

        input_assembly_state_builder &line_strip();

        input_assembly_state_builder &triangles();

        input_assembly_state_builder &triangle_fan();

        input_assembly_state_builder &triangle_strip();

        input_assembly_state_builder &patches();

        input_assembly_state_builder &lines_with_adjacency();

        input_assembly_state_builder &line_strip_with_adjacency();

        input_assembly_state_builder &triangles_with_adjacency();

        input_assembly_state_builder &triangle_strip_with_adjacency();

        VkPipelineInputAssemblyStateCreateInfo &build_input_assembly_state();

        void copy(const input_assembly_state_builder &source);

    private:
        VkPrimitiveTopology _topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        VkBool32 _primitiveRestartEnable{VK_FALSE};
        VkPipelineInputAssemblyStateCreateInfo _info{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

    };
}