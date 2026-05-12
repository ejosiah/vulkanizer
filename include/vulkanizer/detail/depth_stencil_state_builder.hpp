#pragma once

namespace vkz {

    class stencil_op_state_builder;

    class depth_stencil_state_builder : public graphics_pipeline_builder {
    public:
        depth_stencil_state_builder(vkz::device device, graphics_pipeline_builder *parent);

        explicit depth_stencil_state_builder(depth_stencil_state_builder *parent);

        depth_stencil_state_builder &enable_depth_test();

        depth_stencil_state_builder &disable_depth_test();

        depth_stencil_state_builder &enable_depth_write();

        depth_stencil_state_builder &disable_depth_write();

        virtual depth_stencil_state_builder &compare_op_never();

        virtual depth_stencil_state_builder &compare_op_less();

        virtual depth_stencil_state_builder &compare_op_equal();

        virtual depth_stencil_state_builder &compare_op_less_or_equal();

        virtual depth_stencil_state_builder &compare_op_greater();

        virtual depth_stencil_state_builder &compare_op_greater_or_equal();

        virtual depth_stencil_state_builder &compare_op_not_equal();

        virtual depth_stencil_state_builder &compare_op_always();

        depth_stencil_state_builder &enable_depth_bounds_test();

        depth_stencil_state_builder &disable_depth_bounds_test();

        depth_stencil_state_builder &enable_stencil_test();

        depth_stencil_state_builder &disable_stencil_test();

        stencil_op_state_builder &stencil_op_front();

        stencil_op_state_builder &stencil_op_back();

        depth_stencil_state_builder &min_depth_bounds(float value);

        depth_stencil_state_builder &max_depth_bounds(float value);

        VkPipelineDepthStencilStateCreateInfo &build_depth_stencil_state();

        void copy(const depth_stencil_state_builder &source);

    private:
        VkPipelineDepthStencilStateCreateInfo _info;
        stencil_op_state_builder *_front = nullptr;
        stencil_op_state_builder *_back = nullptr;
    };

    class stencil_op_state_builder : public depth_stencil_state_builder {
    public:
        explicit stencil_op_state_builder(depth_stencil_state_builder *parent);

        stencil_op_state_builder &fail_opKeep();

        stencil_op_state_builder &fail_opZero();

        stencil_op_state_builder &fail_opReplace();

        stencil_op_state_builder &fail_opIncrementAndClamp();

        stencil_op_state_builder &fail_opDecrementAndClamp();

        stencil_op_state_builder &fail_opInvert();

        stencil_op_state_builder &fail_opIncrementAndWrap();

        stencil_op_state_builder &fail_opDecrementAndWrap();

        stencil_op_state_builder &pass_opKeep();

        stencil_op_state_builder &pass_opZero();

        stencil_op_state_builder &pass_opReplace();

        stencil_op_state_builder &pass_opIncrementAndClamp();

        stencil_op_state_builder &pass_opDecrementAndClamp();

        stencil_op_state_builder &pass_opInvert();

        stencil_op_state_builder &pass_opIncrementAndWrap();

        stencil_op_state_builder &pass_opDecrementAndWrap();

        stencil_op_state_builder &depth_fail_opKeep();

        stencil_op_state_builder &depth_fail_opZero();

        stencil_op_state_builder &depth_fail_opReplace();

        stencil_op_state_builder &depth_fail_opIncrementAndClamp();

        stencil_op_state_builder &depth_fail_opDecrementAndClamp();

        stencil_op_state_builder &depth_fail_opInvert();

        stencil_op_state_builder &depth_fail_opIncrementAndWrap();

        stencil_op_state_builder &depth_fail_opDecrementAndWrap();

        stencil_op_state_builder &compare_op_never() override;

        stencil_op_state_builder &compare_op_less() override;

        stencil_op_state_builder &compare_op_equal() override;

        stencil_op_state_builder &compare_op_less_or_equal() override;

        stencil_op_state_builder &compare_op_greater() override;

        stencil_op_state_builder &compare_op_greater_or_equal() override;

        stencil_op_state_builder &compare_op_not_equal() override;

        stencil_op_state_builder &compare_op_always() override;

        stencil_op_state_builder &compare_mask(uint32_t value);

        stencil_op_state_builder &write_mask(uint32_t value);

        stencil_op_state_builder &reference(uint32_t value);

        stencil_op_state_builder &clear_stencil_state();

        VkStencilOpState build_stencil_op_state();

        VkStencilOpState _stencil_op_state;
    };

}