#pragma once

namespace vkz {

    class dynamic_render_pass_builder : public graphics_pipeline_builder {
    public:
        explicit dynamic_render_pass_builder(vkz::device device, graphics_pipeline_builder *parent);

        [[nodiscard]]
        dynamic_render_pass_builder &view_mask(uint32_t value);

        dynamic_render_pass_builder &add_color_attachment(VkFormat format);

        dynamic_render_pass_builder &depth_attachment(VkFormat format);

        dynamic_render_pass_builder &stencil_attachment(VkFormat format);

        dynamic_render_pass_builder &enable();

        dynamic_render_pass_builder &disable();

        [[nodiscard]]
        bool enabled() const;

        const VkPipelineRenderingCreateInfo &build_dynamic_render_info();

    private:
        VkPipelineRenderingCreateInfo m_renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        std::vector<VkFormat> m_color_attachments;

        bool m_enabled{};
    };
}