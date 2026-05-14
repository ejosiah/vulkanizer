#pragma once

#include "vkz.hpp"
#include "builder.hpp"

#include <string>
#include <memory>

namespace vkz {

    class graphics_pipeline_builder : public builder_base {
    public:
        friend  class tessellation_state_builder;
        explicit graphics_pipeline_builder(vkz::device device);

        graphics_pipeline_builder(vkz::device device, graphics_pipeline_builder* parent);

        graphics_pipeline_builder() = default;

        graphics_pipeline_builder(graphics_pipeline_builder&& source);

        virtual ~graphics_pipeline_builder();

        virtual shader_stage_builder& shader_stage();

        virtual vertex_input_state_builder& vertex_input_state();

        virtual input_assembly_state_builder& input_assembly_state();

        virtual tessellation_state_builder& tessellation_state();

        virtual viewport_state_builder& viewport_state();

        virtual rasterization_state_builder& rasterization_state();

        virtual depth_stencil_state_builder& depth_stencil_state();

        virtual color_blend_state_builder& color_blend_state(void* next = nullptr);

        virtual multisample_state_builder& multisample_state();

        virtual pipeline_layout_builder& layout();

        virtual dynamic_state_builder& dynamic_state();

        graphics_pipeline_builder& allow_derivatives();

        graphics_pipeline_builder& set_derivatives();

        graphics_pipeline_builder& subpass(uint32_t value);

        graphics_pipeline_builder& layout(VkPipelineLayout  layout);

        graphics_pipeline_builder& render_pass(VkRenderPass  render_pass);

        dynamic_render_pass_builder& dynamic_render_pass();

        graphics_pipeline_builder& name(const std::string& value);

        graphics_pipeline_builder& reuse();

        graphics_pipeline_builder& base_pipeline(VkPipeline& pipeline);

        graphics_pipeline_builder& pipeline_cache(VkPipelineCache pipeline_cache);

        [[nodiscard]]
        graphics_pipeline_builder *parent() override;

        VkPipeline build();

        VkPipeline build(VkPipelineLayout& pipeline_layout);

        VkGraphicsPipelineCreateInfo create_info();

        [[nodiscard]]
        graphics_pipeline_builder clone() const;

        void copy(const graphics_pipeline_builder& source);

        [[nodiscard]]
        VkPipelineLayout pipeline_layout() const {
            return _pipeline_layout_owned;
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
        void* next_chain = nullptr;

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
