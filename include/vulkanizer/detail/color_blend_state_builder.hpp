#pragma once

#include "logic_op.hpp"
#include "blend_factor.hpp"
#include "blend_op.hpp"

namespace vkz {

    class color_blend_attachment_state_builder;

    class color_blend_state_builder : public graphics_pipeline_builder {
    public:
        color_blend_state_builder(vkz::device device, graphics_pipeline_builder *parent);

        ~color_blend_state_builder() override;

        explicit color_blend_state_builder(color_blend_state_builder *parent);

        color_blend_state_builder &blend_constants(float r, float g, float b, float a);

        virtual color_blend_attachment_state_builder &attachment();

        virtual color_blend_attachment_state_builder &attachments(uint32_t count);

        inline logic_op <color_blend_state_builder> &logic_operation() {
            return _logic_op;
        }

        VkPipelineColorBlendStateCreateInfo &build_color_blend_state();

        void copy(const color_blend_state_builder &source);

    private:
        VkPipelineColorBlendStateCreateInfo _info{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        logic_op <color_blend_state_builder> _logic_op{};
        color_blend_attachment_state_builder *_color_blend_attachment_state_builder{nullptr};
    };

    class color_blend_attachment_state_builder : public color_blend_state_builder {
    public:
        explicit color_blend_attachment_state_builder(color_blend_state_builder *parent);

        color_blend_attachment_state_builder &enable_blend();

        color_blend_attachment_state_builder &disable_blend();

        inline blend_factor <color_blend_attachment_state_builder> &src_color_blend_factor() {
            dirty();
            return _src_color_blend_factor;
        }

        inline blend_factor <color_blend_attachment_state_builder> &dst_color_blend_factor() {
            dirty();
            return _dst_color_blend_factor;
        }


        inline blend_factor <color_blend_attachment_state_builder> &src_alpha_blend_factor() {
            dirty();
            return _src_alpha_blend_factor;
        }

        inline blend_factor <color_blend_attachment_state_builder> &dst_alpha_blend_factor() {
            dirty();
            return _dst_alpha_blend_factor;
        }


        inline blend_op <color_blend_attachment_state_builder> &color_blend_op() {
            dirty();
            return _color_blend_op;
        }

        inline blend_op <color_blend_attachment_state_builder> &alpha_blend_op() {
            dirty();
            return _alpha_blend_op;
        }

        color_blend_attachment_state_builder &color_write_mask(VkColorComponentFlags flags);

        color_blend_attachment_state_builder &add();

        color_blend_attachment_state_builder &clear();

        color_blend_attachment_state_builder &attachment() override;


        std::vector<VkPipelineColorBlendAttachmentState> &build_color_blend_attachment_state();

        void copy(const color_blend_attachment_state_builder &source);

    private:
        inline void dirty() {
            _dirty = true;
        }

        void reset_scratchpad();

    private:
        std::vector<VkPipelineColorBlendAttachmentState> _states;
        blend_factor <color_blend_attachment_state_builder> _src_color_blend_factor{};
        blend_factor <color_blend_attachment_state_builder> _dst_color_blend_factor{};
        blend_factor <color_blend_attachment_state_builder> _src_alpha_blend_factor{};
        blend_factor <color_blend_attachment_state_builder> _dst_alpha_blend_factor{};
        blend_op <color_blend_attachment_state_builder> _color_blend_op{};
        blend_op <color_blend_attachment_state_builder> _alpha_blend_op{};
        VkPipelineColorBlendAttachmentState _scratchpad;
        bool _dirty = false;
    };
}