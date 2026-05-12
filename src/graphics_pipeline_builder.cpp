#include "vulkanizer/graphics_pipeline_builder.hpp"
#include "vulkanizer/struct_mapping.hpp"

namespace vkz {

    graphics_pipeline_builder::graphics_pipeline_builder(vkz::device device)
            : builder_base{device, nullptr}, _shader_stage_builder{std::make_unique<shader_stage_builder>(device, this)},
              _vertex_input_state_builder{std::make_unique<vertex_input_state_builder>(device, this)},
              _input_assembly_state_builder{std::make_unique<input_assembly_state_builder>(device, this)},
              _pipeline_layout_builder{std::make_unique<pipeline_layout_builder>(device, this)},
              _viewport_state_builder{std::make_unique<viewport_state_builder>(device, this)},
              _rasterization_state_builder{std::make_unique<rasterization_state_builder>(device, this)},
              _multisample_state_builder{std::make_unique<multisample_state_builder>(device, this)},
              _depth_stencil_state_builder{std::make_unique<depth_stencil_state_builder>(device, this)},
              _color_blend_state_builder{std::make_unique<color_blend_state_builder>(device, this)},
              _dynamic_state_builder{std::make_unique<dynamic_state_builder>(device, this)},
              _tessellation_state_builder{std::make_unique<tessellation_state_builder>(device, this)},
              _dynamic_render_state_builder{std::make_unique<dynamic_render_pass_builder>(device, this)} {}

    graphics_pipeline_builder::graphics_pipeline_builder(vkz::device device, graphics_pipeline_builder *parent)
            : builder_base{device, parent} {
    }

    graphics_pipeline_builder::graphics_pipeline_builder(graphics_pipeline_builder &&source) {
        _shader_stage_builder = std::move(source._shader_stage_builder);
        _vertex_input_state_builder = std::move(source._vertex_input_state_builder);
        _input_assembly_state_builder = std::move(source._input_assembly_state_builder);
        _pipeline_layout_builder = std::move(source._pipeline_layout_builder);
        _viewport_state_builder = std::move(source._viewport_state_builder);
        _rasterization_state_builder = std::move(source._rasterization_state_builder);
        _multisample_state_builder = std::move(source._multisample_state_builder);
        _depth_stencil_state_builder = std::move(source._depth_stencil_state_builder);
        _color_blend_state_builder = std::move(source._color_blend_state_builder);
        _dynamic_state_builder = std::move(source._dynamic_state_builder);
        _tessellation_state_builder = std::move(source._tessellation_state_builder);
        _name = std::move(source._name);
        _flags = source._flags;
        _render_pass = std::exchange(source._render_pass, VK_NULL_HANDLE);
        _pipeline_layout = std::move(source._pipeline_layout);
        _pipeline_layout_owned = std::move(source._pipeline_layout_owned);
        _subpass = source._subpass;
        _base_pipeline = std::move(source._base_pipeline);
        _pipeline_cache = std::move(source._pipeline_cache);
        next_chain = std::exchange(source.next_chain, nullptr);
        _parent = std::exchange(source._parent, nullptr);
        _device = source._device;
    }


    graphics_pipeline_builder::~graphics_pipeline_builder() = default;

    shader_stage_builder &graphics_pipeline_builder::shader_stage() {
        if (parent()) {
            return parent()->shader_stage();
        }
        return *_shader_stage_builder;
    }

    vertex_input_state_builder &graphics_pipeline_builder::vertex_input_state() {
        if (parent()) {
            return parent()->vertex_input_state();
        }
        return *_vertex_input_state_builder;
    }

    graphics_pipeline_builder *graphics_pipeline_builder::parent() {
        return dynamic_cast<graphics_pipeline_builder *>(builder_base::parent());
    }

    input_assembly_state_builder &graphics_pipeline_builder::input_assembly_state() {
        if (parent()) {
            return parent()->input_assembly_state();
        }
        return *_input_assembly_state_builder;
    }

    tessellation_state_builder &graphics_pipeline_builder::tessellation_state() {
        if (parent()) {
            return parent()->tessellation_state();
        }
        return *_tessellation_state_builder;
    }

    graphics_pipeline_builder &graphics_pipeline_builder::allow_derivatives() {
        if (parent()) {
            return parent()->allow_derivatives();
        }
        _flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
        return *this;
    }

    graphics_pipeline_builder &graphics_pipeline_builder::set_derivatives() {
        if (parent()) {
            return parent()->set_derivatives();
        }
        _flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        return *this;
    }

    graphics_pipeline_builder &graphics_pipeline_builder::subpass(uint32_t value) {
        if (parent()) {
            return parent()->subpass(value);
        }
        _subpass = value;
        return *this;
    }


    graphics_pipeline_builder &graphics_pipeline_builder::layout(VkPipelineLayout layout) {
        if (parent()) {
            return parent()->layout(layout);
        }
        _pipeline_layout = layout;
        return *this;
    }

    graphics_pipeline_builder &graphics_pipeline_builder::render_pass(VkRenderPass render_pass) {
        if (parent()) {
            return parent()->render_pass(render_pass);
        }
        _render_pass = render_pass;
        return *this;
    }

    dynamic_render_pass_builder &graphics_pipeline_builder::dynamic_render_pass() {
        if (parent()) {
            return parent()->dynamic_render_pass();
        }
        _render_pass = VK_NULL_HANDLE;
        _dynamic_render_state_builder->enable();
        return *_dynamic_render_state_builder;
    }

    pipeline_layout_builder &graphics_pipeline_builder::layout() {
        if (parent()) {
            return parent()->layout();
        }
        return *_pipeline_layout_builder;
    }

    VkPipeline graphics_pipeline_builder::build() {
        if (parent()) {
            return parent()->build();
        }
        if (!_pipeline_layout) {
            throw std::runtime_error{"either provide or create a pipeline_layout"};
        }
        VkPipelineLayout unused{};
        return build(unused);
    }


    VkPipeline graphics_pipeline_builder::build(VkPipelineLayout &pipeline_layout) {
        if (parent()) {
            return parent()->build(pipeline_layout);
        }
        auto info = create_info();
        pipeline_layout = std::move(_pipeline_layout_owned);

        VkPipeline pipeline;
        VKZ_CHECK_VULKAN(vkCreateGraphicsPipelines(device().logical, nullptr, 1, &info, nullptr, &pipeline));
        if (!_name.empty()) {
           set_name<VK_OBJECT_TYPE_PIPELINE>(device(), _name, pipeline);
           set_name<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(device(), _name, pipeline_layout);
        }
        _shader_stage_builder->clear_stages();
        return pipeline;
    }

    VkGraphicsPipelineCreateInfo graphics_pipeline_builder::create_info() {
        if (parent()) return parent()->create_info();

        auto &shader_stages = _shader_stage_builder->build_shader_stage();
        auto &vertex_input_state = _vertex_input_state_builder->build_vertex_input_state();
        auto &input_assembly_state = _input_assembly_state_builder->build_input_assembly_state();
        auto &viewport_state = _viewport_state_builder->build_viewport_state();
        auto &rasterState = _rasterization_state_builder->build_raster_state();
        auto &multisample_state = _multisample_state_builder->build_multisample_state();
        auto &depth_stencil_state = _depth_stencil_state_builder->build_depth_stencil_state();
        auto &color_blend_state = _color_blend_state_builder->build_color_blend_state();
        auto &dynamic_state = _dynamic_state_builder->build_pipeline_dynamic_state();
        auto &tessellation_state = _tessellation_state_builder->build_tessellation_state();

        auto info = makeStruct<VkGraphicsPipelineCreateInfo>();
        info.flags = _flags;
        info.stageCount = VKZ_COUNT(shader_stages);
        info.pStages = shader_stages.data();
        info.pVertexInputState = &vertex_input_state;
        info.pInputAssemblyState = &input_assembly_state;
        info.pTessellationState = &tessellation_state;
        info.pViewportState = &viewport_state;
        info.pRasterizationState = &rasterState;
        info.pMultisampleState = &multisample_state;
        info.pDepthStencilState = &depth_stencil_state;
        info.pColorBlendState = &color_blend_state;
        info.pDynamicState = &dynamic_state;

        if (_flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT) {
            assert(_base_pipeline);
            info.basePipelineHandle = _base_pipeline;
            info.basePipelineIndex = -1;
        }

        if (!_pipeline_layout) {
            _pipeline_layout_owned = _pipeline_layout_builder->build_pipeline_layout();
            info.layout = _pipeline_layout_owned;
        } else {
            info.layout = _pipeline_layout;
        }
        info.renderPass = _render_pass;
        info.subpass = _subpass;

        if (_dynamic_render_state_builder->enabled()) {
            info.pNext = &_dynamic_render_state_builder->build_dynamic_render_info();
        }

        return info;
    }

    viewport_state_builder &graphics_pipeline_builder::viewport_state() {
        if (parent()) {
            return parent()->viewport_state();
        }
        return *_viewport_state_builder;
    }

    rasterization_state_builder &graphics_pipeline_builder::rasterization_state() {
        if (parent()) {
            return parent()->rasterization_state();
        }
        return *_rasterization_state_builder;
    }

    depth_stencil_state_builder &graphics_pipeline_builder::depth_stencil_state() {
        if (parent()) {
            return parent()->depth_stencil_state();
        }
        return *_depth_stencil_state_builder;
    }

    color_blend_state_builder &graphics_pipeline_builder::color_blend_state(void *next) {
        if (parent()) {
            return parent()->color_blend_state(next);
        }
        _color_blend_state_builder->next_chain = next;
        return *_color_blend_state_builder;
    }

    graphics_pipeline_builder &graphics_pipeline_builder::name(const std::string &value) {
        if (parent()) {
            parent()->name(value);
        }
        _name = value;
        return *this;
    }

    graphics_pipeline_builder &graphics_pipeline_builder::reuse() {
        if (parent()) {
            parent()->reuse();
        }
        _vertex_input_state_builder->clear();
        _shader_stage_builder->clear();
        _pipeline_layout_builder->clear_layouts();
        _pipeline_layout_builder->clear_ranges();
        return *this;
    }

    graphics_pipeline_builder &graphics_pipeline_builder::base_pipeline(VkPipeline &pipeline) {
        set_derivatives();
        if (parent()) {
            parent()->base_pipeline(pipeline);
        }
        _base_pipeline = pipeline;
        return *this;
    }

    multisample_state_builder &graphics_pipeline_builder::multisample_state() {
        if (parent()) {
            return parent()->multisample_state();
        }
        return *_multisample_state_builder;
    }

    graphics_pipeline_builder &graphics_pipeline_builder::pipeline_cache(VkPipelineCache pipeline_cache) {
        if (parent()) {
            parent()->pipeline_cache(pipeline_cache);
        }
        _pipeline_cache = pipeline_cache;
        return *this;
    }

    graphics_pipeline_builder graphics_pipeline_builder::clone() const {
        graphics_pipeline_builder aClone{_device};
        aClone.copy(*this);

        return aClone;
    }

    dynamic_state_builder &graphics_pipeline_builder::dynamic_state() {
        if (parent()) {
            return parent()->dynamic_state();
        }
        return *_dynamic_state_builder;
    }


    void graphics_pipeline_builder::copy(const graphics_pipeline_builder &source) {
        _flags = source._flags;
        _render_pass = source._render_pass;
        _pipeline_layout = source._pipeline_layout;
        _pipeline_layout_owned = source._pipeline_layout_owned;
        _subpass = source._subpass;
        _name = source._name;

        _shader_stage_builder->copy(*source._shader_stage_builder);
        _vertex_input_state_builder->copy(*source._vertex_input_state_builder);
        _input_assembly_state_builder->copy(*source._input_assembly_state_builder);
        _pipeline_layout_builder->copy(*source._pipeline_layout_builder);
        _viewport_state_builder->copy(*source._viewport_state_builder);
        _rasterization_state_builder->copy(*source._rasterization_state_builder);
        _multisample_state_builder->copy(*source._multisample_state_builder);
        _depth_stencil_state_builder->copy(*source._depth_stencil_state_builder);
        _color_blend_state_builder->copy(*source._color_blend_state_builder);
        _dynamic_state_builder->copy(*source._dynamic_state_builder);
        _tessellation_state_builder->copy(*source._tessellation_state_builder);

        _base_pipeline = source._base_pipeline;
        _pipeline_cache = source._pipeline_cache;
        next_chain = source.next_chain;
    }

}