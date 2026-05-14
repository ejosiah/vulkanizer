#pragma once

namespace vkz {

    class dynamic_state_builder : public graphics_pipeline_builder {
    public:
        dynamic_state_builder(vkz::device device, graphics_pipeline_builder *parent);

        dynamic_state_builder &viewport();

        dynamic_state_builder &scissor();

        dynamic_state_builder &line_width();

        dynamic_state_builder &depth_bias();

        dynamic_state_builder &blend_constants();

        dynamic_state_builder &depth_bounds();

        dynamic_state_builder &stencil_compare_mask();

        dynamic_state_builder &stencil_write_mask();

        dynamic_state_builder &stencil_reference_mask();

        dynamic_state_builder &cull_mode();

        dynamic_state_builder &front_face();

        dynamic_state_builder &primitive_topology();

        dynamic_state_builder &viewport_with_count();

        dynamic_state_builder &scissor_with_count();

        dynamic_state_builder &vertex_input_binding_stride();

        dynamic_state_builder &depth_test_enable();

        dynamic_state_builder &depth_write_enable();

        dynamic_state_builder &depth_compare_op();

        dynamic_state_builder &depth_bounds_test_enable();

        dynamic_state_builder &stencil_test_enable();

        dynamic_state_builder &stencil_op();

        dynamic_state_builder &raster_discard_enable();

        dynamic_state_builder &depth_bias_enable();

        dynamic_state_builder &primitive_restart_enable();

        dynamic_state_builder &color_write_enable();

        dynamic_state_builder &polygon_mode_enable();

        dynamic_state_builder &color_blend_enable();

        dynamic_state_builder &clear();

        VkPipelineDynamicStateCreateInfo &build_pipeline_dynamic_state();

        void copy(const dynamic_state_builder &source);

    private:
        std::vector<VkDynamicState> _dynamic_states;
        VkPipelineDynamicStateCreateInfo _info;
    };
}