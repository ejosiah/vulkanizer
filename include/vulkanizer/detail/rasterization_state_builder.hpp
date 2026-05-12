#pragma once

namespace vkz {

    class rasterization_state_builder : public graphics_pipeline_builder {
    public:
        rasterization_state_builder(vkz::device device, graphics_pipeline_builder *parent);

        rasterization_state_builder &enable_depth_clamp();

        rasterization_state_builder &disable_depth_clamp();

        rasterization_state_builder &enable_rasterizer_discard();

        rasterization_state_builder &disable_rasterizer_discard();

        rasterization_state_builder &polygon_mode_fill();

        rasterization_state_builder &polygon_mode_line();

        rasterization_state_builder &polygon_mode_point();

        rasterization_state_builder &cull_none();

        rasterization_state_builder &cull_front_face();

        rasterization_state_builder &cull_back_face();

        rasterization_state_builder &cull_front_and_back_face();

        rasterization_state_builder &front_face_counter_clockwise();

        rasterization_state_builder &front_face_clockwise();

        rasterization_state_builder &enable_depth_bias();

        rasterization_state_builder &disable_depth_bias();

        rasterization_state_builder &depth_bias_constant_factor(float value);

        rasterization_state_builder &depth_bias_clamp(float value);

        rasterization_state_builder &depth_bias_slope_factor(float value);

        rasterization_state_builder &line_width(float value);

        VkPipelineRasterizationStateCreateInfo &build_raster_state();

        void copy(const rasterization_state_builder &source);

    private:
        VkPipelineRasterizationStateCreateInfo _info;
    };

}