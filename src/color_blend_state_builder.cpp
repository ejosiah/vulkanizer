#include "vulkanizer/graphics_pipeline_builder.hpp"

namespace vkz {

    color_blend_state_builder::color_blend_state_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent),
              _color_blend_attachment_state_builder{new color_blend_attachment_state_builder{this}},
              _logic_op{this} {}

    color_blend_state_builder::color_blend_state_builder(color_blend_state_builder *parent)
            : graphics_pipeline_builder(parent->_device, parent) {

    }

    color_blend_state_builder::~color_blend_state_builder() {
        delete _color_blend_attachment_state_builder;
    }

    color_blend_state_builder &color_blend_state_builder::blend_constants(float r, float g, float b, float a) {
        _info.blendConstants[0] = r;
        _info.blendConstants[1] = g;
        _info.blendConstants[2] = b;
        _info.blendConstants[3] = a;
        return *this;
    }

    color_blend_attachment_state_builder &color_blend_state_builder::attachment() {
        return *_color_blend_attachment_state_builder;
    }

    color_blend_attachment_state_builder &color_blend_state_builder::attachments(uint32_t count) {
        auto builder = _color_blend_attachment_state_builder;
        builder->clear();
        for (int i = 0; i < count; i++) {
            builder->add();
        }
        return *builder;
    }

    VkPipelineColorBlendStateCreateInfo &color_blend_state_builder::build_color_blend_state() {
        auto &color_attachment_states = _color_blend_attachment_state_builder->build_color_blend_attachment_state();
        _info.attachmentCount = VKZ_COUNT(color_attachment_states);
        _info.pAttachments = color_attachment_states.data();
        _info.logicOpEnable = _logic_op.enabled;
        _info.logicOp = _logic_op.value;
        return _info;
    }

    void color_blend_state_builder::copy(const color_blend_state_builder &source) {
        _color_blend_attachment_state_builder->copy(*source._color_blend_attachment_state_builder);
        _logic_op = source._logic_op;
        _info = source._info;
    }


    color_blend_attachment_state_builder::color_blend_attachment_state_builder(color_blend_state_builder *parent)
            : color_blend_state_builder(parent) {
        _src_color_blend_factor._caller = this;
        _dst_color_blend_factor._caller = this;
        _src_alpha_blend_factor._caller = this;
        _dst_alpha_blend_factor._caller = this;
        _color_blend_op._caller = this;
        _alpha_blend_op._caller = this;
        reset_scratchpad();
    }

    color_blend_attachment_state_builder &color_blend_attachment_state_builder::enable_blend() {
        dirty();
        _scratchpad.blendEnable = VK_TRUE;
        return *this;
    }

    color_blend_attachment_state_builder &color_blend_attachment_state_builder::disable_blend() {
        dirty();
        _scratchpad.blendEnable = VK_FALSE;
        return *this;
    }

    color_blend_attachment_state_builder &color_blend_attachment_state_builder::color_write_mask(VkColorComponentFlags flags) {
        dirty();
        _scratchpad.colorWriteMask = flags;
        return *this;
    }

    void color_blend_attachment_state_builder::reset_scratchpad() {
        _dirty = false;
        _scratchpad = VkPipelineColorBlendAttachmentState{};
        _scratchpad.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;
    }

    color_blend_attachment_state_builder &color_blend_attachment_state_builder::add() {
        _scratchpad.srcColorBlendFactor = _src_color_blend_factor.value;
        _scratchpad.dstColorBlendFactor = _dst_color_blend_factor.value;
        _scratchpad.colorBlendOp = _color_blend_op.value;
        _scratchpad.srcAlphaBlendFactor = _src_alpha_blend_factor.value;
        _scratchpad.dstAlphaBlendFactor = _dst_alpha_blend_factor.value;
        _scratchpad.alphaBlendOp = _alpha_blend_op.value;
        _states.push_back(_scratchpad);
        reset_scratchpad();
        return *this;
    }

    std::vector<VkPipelineColorBlendAttachmentState> &
    color_blend_attachment_state_builder::build_color_blend_attachment_state() {
        return _states;
    }

    void color_blend_attachment_state_builder::copy(const color_blend_attachment_state_builder &source) {
        _states = decltype(_states)(source._states.begin(), source._states.end());
    }

    color_blend_attachment_state_builder &color_blend_attachment_state_builder::attachment() {
        if (_dirty) {
            add();
        }
        return *this;
    }

    color_blend_attachment_state_builder &color_blend_attachment_state_builder::clear() {
        _states.clear();
        reset_scratchpad();
        return *this;
    }

}