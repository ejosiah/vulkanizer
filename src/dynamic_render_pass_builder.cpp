#include "vulkanizer/graphics_pipeline_builder.hpp"
#include "vulkanizer/log.hpp"
namespace vkz {

    dynamic_render_pass_builder::dynamic_render_pass_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent) {}

    dynamic_render_pass_builder &dynamic_render_pass_builder::view_mask(uint32_t value) {
        m_renderingCreateInfo.viewMask = value;
        return *this;
    }

    dynamic_render_pass_builder &dynamic_render_pass_builder::add_color_attachment(VkFormat format) {
        m_color_attachments.push_back(format);
        return *this;
    }

    dynamic_render_pass_builder &dynamic_render_pass_builder::depth_attachment(VkFormat format) {
        m_renderingCreateInfo.depthAttachmentFormat = format;
        return *this;
    }

    dynamic_render_pass_builder &dynamic_render_pass_builder::stencil_attachment(VkFormat format) {
        m_renderingCreateInfo.stencilAttachmentFormat = format;
        return *this;
    }

    dynamic_render_pass_builder &dynamic_render_pass_builder::enable() {
        m_enabled = true;
        m_color_attachments.clear();
        m_renderingCreateInfo = VkPipelineRenderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        return *this;
    }

    dynamic_render_pass_builder &dynamic_render_pass_builder::disable() {
        m_enabled = false;
        m_color_attachments.clear();
        m_renderingCreateInfo = VkPipelineRenderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        return *this;
    }

    bool dynamic_render_pass_builder::enabled() const {
        return m_enabled;
    }

    const VkPipelineRenderingCreateInfo &dynamic_render_pass_builder::build_dynamic_render_info() {
        if (m_color_attachments.empty() && m_renderingCreateInfo.depthAttachmentFormat != VK_FORMAT_UNDEFINED) {
            vkz::warn("you may not have a depth buffer or color render target defined");
        }
        m_renderingCreateInfo.colorAttachmentCount = m_color_attachments.size();
        m_renderingCreateInfo.pColorAttachmentFormats = m_color_attachments.data();


        return m_renderingCreateInfo;
    }
}