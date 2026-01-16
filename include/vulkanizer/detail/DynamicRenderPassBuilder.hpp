#pragma once

namespace vkz {

    class DynamicRenderPassBuilder : public GraphicsPipelineBuilder {
    public:
        explicit DynamicRenderPassBuilder(Device device, GraphicsPipelineBuilder *parent);

        [[nodiscard]]
        DynamicRenderPassBuilder &viewMask(uint32_t value);

        DynamicRenderPassBuilder &addColorAttachment(VkFormat format);

        DynamicRenderPassBuilder &depthAttachment(VkFormat format);

        DynamicRenderPassBuilder &stencilAttachment(VkFormat format);

        DynamicRenderPassBuilder &enable();

        DynamicRenderPassBuilder &disable();

        [[nodiscard]]
        bool enabled() const;

        const VkPipelineRenderingCreateInfo &buildDynamicRenderInfo();

    private:
        VkPipelineRenderingCreateInfo m_renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        std::vector<VkFormat> m_colorAttachments;

        bool m_enabled{};
    };
}