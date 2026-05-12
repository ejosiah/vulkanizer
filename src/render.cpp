#include "vulkanizer/render.hpp"

namespace vkz {
    void render(VkCommandBuffer command_buffer, const render_info& render_data, scene scene) {
        VkRenderingInfo info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        info.flags = 0;
        info.renderArea = {{0, 0}, {render_data.render_area.x, render_data.render_area.y}};
        info.layerCount = render_data.num_layers;
        info.viewMask = render_data.view_mask;

        std::vector<VkRenderingAttachmentInfo> color_attachments;
        for(const auto& [view, format, clear_value, resolve, clear] : render_data.color_attachments) {
            VkRenderingAttachmentInfo attachment_info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment_info.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment_info.clearValue.color = {clear_value.r, clear_value.g, clear_value.b, clear_value.a};
            attachment_info.imageView = view.handle;

            if (resolve.has_value()) {
                attachment_info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                attachment_info.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachment_info.resolveImageView = resolve->handle;
            }

            color_attachments.push_back(attachment_info);
        }

        info.colorAttachmentCount = color_attachments.size();
        info.pColorAttachments = color_attachments.data();

        VkRenderingAttachmentInfo depth_attachment_info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        if(render_data.depth_attachment.has_value()) {
            auto [view, format, clear_value, clear]  = *render_data.depth_attachment;

            depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depth_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
            depth_attachment_info.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depth_attachment_info.clearValue.depthStencil = {clear_value.x, static_cast<uint32_t>(clear_value.y)};
            depth_attachment_info.imageView = view.handle;

            info.pDepthAttachment = &depth_attachment_info;
        }

        VkRenderingAttachmentInfo stencil_attachment_info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        if(render_data.stencil_attachment.has_value()) {
            auto [view, format, clear_value, clear] = *render_data.stencil_attachment;

            stencil_attachment_info.imageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            stencil_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
            stencil_attachment_info.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            stencil_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            stencil_attachment_info.clearValue.depthStencil = {clear_value.x, static_cast<uint32_t>(clear_value.y)};
            stencil_attachment_info.imageView = view.handle;

            info.pStencilAttachment = &stencil_attachment_info;
        }

        vkCmdBeginRendering(command_buffer, &info);
        scene();
        vkCmdEndRendering(command_buffer);
    }
}
