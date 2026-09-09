#pragma once

#include "vkz.hpp"
#include "builder.hpp"
#include "pipeline.hpp"

#include <string>
#include <memory>

namespace vkz {

    template <typename Derived> class graphics_pipeline_builder_proxy {
    public:
        explicit graphics_pipeline_builder_proxy(graphics_pipeline_builder *builder)
            : _builder{builder} {}

        shader_stage_builder &shader_stage();
        vertex_input_state_builder &vertex_input_state();
        input_assembly_state_builder &input_assembly_state();
        tessellation_state_builder &tessellation_state();
        viewport_state_builder &viewport_state();
        rasterization_state_builder &rasterization_state();
        depth_stencil_state_builder &depth_stencil_state();
        color_blend_state_builder &color_blend_state(void *next = nullptr);
        multisample_state_builder &multisample_state();
        pipeline_layout_builder &layout();
        dynamic_state_builder &dynamic_state();
        dynamic_render_pass_builder &dynamic_render_pass();

        Derived &allow_derivatives();
        Derived &set_derivatives();
        Derived &subpass(uint32_t value);
        Derived &layout(const vkz::pipeline &pipeline);
        Derived &render_pass(VkRenderPass render_pass);
        Derived &name(const std::string &value);
        Derived &reuse();
        Derived &base_pipeline(const vkz::pipeline &pipeline);
        Derived &pipeline_cache(VkPipelineCache pipeline_cache);

        [[nodiscard]] VkPipeline build_native();
        [[nodiscard]] VkPipeline build(VkPipelineLayout &pipeline_layout);
        [[nodiscard]] vkz::pipeline build();
        [[nodiscard]] VkGraphicsPipelineCreateInfo create_info();
        [[nodiscard]] vkz::device device() const;

        void rebind(graphics_pipeline_builder *builder) {
            _builder = builder;
        }

    protected:
        [[nodiscard]] Derived &derived() {
            return static_cast<Derived &>(*this);
        }

        graphics_pipeline_builder *_builder{};
    };

    class graphics_pipeline_builder {
    public:
        friend  class tessellation_state_builder;
        explicit graphics_pipeline_builder(vkz::device device);

        graphics_pipeline_builder() = default;

        graphics_pipeline_builder(graphics_pipeline_builder&& source);

        ~graphics_pipeline_builder();

        shader_stage_builder& shader_stage();

        vertex_input_state_builder& vertex_input_state();

        input_assembly_state_builder& input_assembly_state();

        tessellation_state_builder& tessellation_state();

        viewport_state_builder& viewport_state();

        rasterization_state_builder& rasterization_state();

        depth_stencil_state_builder& depth_stencil_state();

        color_blend_state_builder& color_blend_state(void* next = nullptr);

        multisample_state_builder& multisample_state();

        pipeline_layout_builder& layout();

        dynamic_state_builder& dynamic_state();

        graphics_pipeline_builder& allow_derivatives();

        graphics_pipeline_builder& set_derivatives();

        graphics_pipeline_builder& subpass(uint32_t value);

        graphics_pipeline_builder& layout(const vkz::pipeline& pipeline);

        graphics_pipeline_builder& render_pass(VkRenderPass  render_pass);

        dynamic_render_pass_builder& dynamic_render_pass();

        graphics_pipeline_builder& name(const std::string& value);

        graphics_pipeline_builder& reuse();

        graphics_pipeline_builder& base_pipeline(const vkz::pipeline& pipeline);

        graphics_pipeline_builder& pipeline_cache(VkPipelineCache pipeline_cache);

        // TODO do we need this?
        [[nodiscard]] VkPipeline build_native();

        [[nodiscard]] VkPipeline build(VkPipelineLayout& pipeline_layout);

        [[nodiscard]] vkz::pipeline build();

        [[nodiscard]] VkGraphicsPipelineCreateInfo create_info();

        graphics_pipeline_builder clone() const;

        void copy(const graphics_pipeline_builder& source);

        VkPipelineLayout pipeline_layout() const {
            return _pipeline_layout_owned;
        }

        vkz::device device() const {
            return _device;
        }

    protected:
        VkPipelineCreateFlags _flags = 0;
        VkRenderPass _render_pass{};
        VkPipelineLayout _pipeline_layout{};
        VkPipelineLayout _pipeline_layout_owned{};
        uint32_t _subpass = 0;
        std::string _name;

        std::unique_ptr<shader_stage_builder> _shader_stage_builder = nullptr;
        std::unique_ptr<vertex_input_state_builder> _vertex_input_state_builder = nullptr;
        std::unique_ptr<input_assembly_state_builder> _input_assembly_state_builder = nullptr;
        std::unique_ptr<pipeline_layout_builder> _pipeline_layout_builder = nullptr;
        std::unique_ptr<viewport_state_builder> _viewport_state_builder = nullptr;
        std::unique_ptr<rasterization_state_builder> _rasterization_state_builder = nullptr;
        std::unique_ptr<multisample_state_builder> _multisample_state_builder = nullptr;
        std::unique_ptr<depth_stencil_state_builder> _depth_stencil_state_builder = nullptr;
        std::unique_ptr<color_blend_state_builder> _color_blend_state_builder = nullptr ;
        std::unique_ptr<dynamic_state_builder> _dynamic_state_builder = nullptr;
        std::unique_ptr<tessellation_state_builder> _tessellation_state_builder = nullptr;
        std::unique_ptr<dynamic_render_pass_builder> _dynamic_render_state_builder = nullptr;

        VkPipeline _base_pipeline{};
        VkPipelineCache _pipeline_cache{};
        vkz::device _device{};

    };
}
#include "detail/shader_stage_builder.hpp"
#include "detail/vertex_input_state_builder.hpp"
#include "detail/input_assembly_state_builder.hpp"
#include "detail/pipeline_layout_builder.hpp"
#include "detail/viewport_state_builder.hpp"
#include "detail/rasterization_state_builder.hpp"
#include "detail/multisample_state_builder.hpp"
#include "detail/depth_stencil_state_builder.hpp"
#include "detail/color_blend_state_builder.hpp"
#include "detail/dynamic_state_builder.hpp"
#include "detail/tessellation_state_builder.hpp"
#include "detail/dynamic_render_pass_builder.hpp"

namespace vkz {

    template <typename Derived> shader_stage_builder &graphics_pipeline_builder_proxy<Derived>::shader_stage() { return _builder->shader_stage(); }
    template <typename Derived> vertex_input_state_builder &graphics_pipeline_builder_proxy<Derived>::vertex_input_state() { return _builder->vertex_input_state(); }
    template <typename Derived> input_assembly_state_builder &graphics_pipeline_builder_proxy<Derived>::input_assembly_state() { return _builder->input_assembly_state(); }
    template <typename Derived> tessellation_state_builder &graphics_pipeline_builder_proxy<Derived>::tessellation_state() { return _builder->tessellation_state(); }
    template <typename Derived> viewport_state_builder &graphics_pipeline_builder_proxy<Derived>::viewport_state() { return _builder->viewport_state(); }
    template <typename Derived> rasterization_state_builder &graphics_pipeline_builder_proxy<Derived>::rasterization_state() { return _builder->rasterization_state(); }
    template <typename Derived> depth_stencil_state_builder &graphics_pipeline_builder_proxy<Derived>::depth_stencil_state() { return _builder->depth_stencil_state(); }
    template <typename Derived> color_blend_state_builder &graphics_pipeline_builder_proxy<Derived>::color_blend_state(void *next) { return _builder->color_blend_state(next); }
    template <typename Derived> multisample_state_builder &graphics_pipeline_builder_proxy<Derived>::multisample_state() { return _builder->multisample_state(); }
    template <typename Derived> pipeline_layout_builder &graphics_pipeline_builder_proxy<Derived>::layout() { return _builder->layout(); }
    template <typename Derived> dynamic_state_builder &graphics_pipeline_builder_proxy<Derived>::dynamic_state() { return _builder->dynamic_state(); }
    template <typename Derived> dynamic_render_pass_builder &graphics_pipeline_builder_proxy<Derived>::dynamic_render_pass() { return _builder->dynamic_render_pass(); }
    template <typename Derived> Derived &graphics_pipeline_builder_proxy<Derived>::allow_derivatives() { _builder->allow_derivatives(); return derived(); }
    template <typename Derived> Derived &graphics_pipeline_builder_proxy<Derived>::set_derivatives() { _builder->set_derivatives(); return derived(); }
    template <typename Derived> Derived &graphics_pipeline_builder_proxy<Derived>::subpass(uint32_t value) { _builder->subpass(value); return derived(); }
    template <typename Derived> Derived &graphics_pipeline_builder_proxy<Derived>::layout(const vkz::pipeline &pipeline) { _builder->layout(pipeline); return derived(); }
    template <typename Derived> Derived &graphics_pipeline_builder_proxy<Derived>::render_pass(VkRenderPass render_pass) { _builder->render_pass(render_pass); return derived(); }
    template <typename Derived> Derived &graphics_pipeline_builder_proxy<Derived>::name(const std::string &value) { _builder->name(value); return derived(); }
    template <typename Derived> Derived &graphics_pipeline_builder_proxy<Derived>::reuse() { _builder->reuse(); return derived(); }
    template <typename Derived> Derived &graphics_pipeline_builder_proxy<Derived>::base_pipeline(const vkz::pipeline &pipeline) { _builder->base_pipeline(pipeline); return derived(); }
    template <typename Derived> Derived &graphics_pipeline_builder_proxy<Derived>::pipeline_cache(VkPipelineCache pipeline_cache) { _builder->pipeline_cache(pipeline_cache); return derived(); }
    template <typename Derived> VkPipeline graphics_pipeline_builder_proxy<Derived>::build_native() { return _builder->build_native(); }
    template <typename Derived> VkPipeline graphics_pipeline_builder_proxy<Derived>::build(VkPipelineLayout &pipeline_layout) { return _builder->build(pipeline_layout); }
    template <typename Derived> vkz::pipeline graphics_pipeline_builder_proxy<Derived>::build() { return _builder->build(); }
    template <typename Derived> VkGraphicsPipelineCreateInfo graphics_pipeline_builder_proxy<Derived>::create_info() { return _builder->create_info(); }
    template <typename Derived> vkz::device graphics_pipeline_builder_proxy<Derived>::device() const { return _builder->device(); }

}
