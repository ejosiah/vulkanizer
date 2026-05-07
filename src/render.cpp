#include "vulkanizer/render.hpp"

namespace vkz {
    void render(VkCommandBuffer commandBuffer, const render_info& renderInfo, scene scene) {
        VkRenderingInfo info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        info.flags = 0;
        info.renderArea = {{0, 0}, {renderInfo.renderArea.x, renderInfo.renderArea.y}};
        info.layerCount = renderInfo.numLayers;
        info.viewMask = renderInfo.viewMask;

        std::vector<VkRenderingAttachmentInfo> colorAttachments;
        for(const auto& [imageView, format, cv, resolve, clear] : renderInfo.colorAttachments) {
            VkRenderingAttachmentInfo attachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentInfo.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachmentInfo.clearValue.color = {cv.r, cv.g, cv.b, cv.a};
            attachmentInfo.imageView = imageView.handle;

            if (resolve.has_value()) {
                attachmentInfo.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                attachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachmentInfo.resolveImageView = resolve->handle;
            }

            colorAttachments.push_back(attachmentInfo);
        }

        info.colorAttachmentCount = colorAttachments.size();
        info.pColorAttachments = colorAttachments.data();

        VkRenderingAttachmentInfo depthAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        if(renderInfo.depthAttachment.has_value()) {
            auto [imageView, format, cv, clear]  = *renderInfo.depthAttachment;

            depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
            depthAttachmentInfo.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachmentInfo.clearValue.depthStencil = {cv.x, static_cast<uint32_t>(cv.y)};
            depthAttachmentInfo.imageView = imageView.handle;

            info.pDepthAttachment = &depthAttachmentInfo;
        }

        VkRenderingAttachmentInfo stencilAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        if(renderInfo.stencilAttachment.has_value()) {
            auto [imageView, format, cv, clear] = *renderInfo.stencilAttachment;

            stencilAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            stencilAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
            stencilAttachmentInfo.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            stencilAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            stencilAttachmentInfo.clearValue.depthStencil = {cv.x, static_cast<uint32_t>(cv.y)};
            stencilAttachmentInfo.imageView = imageView.handle;

            info.pStencilAttachment = &stencilAttachmentInfo;
        }

        vkCmdBeginRendering(commandBuffer, &info);
        scene();
        vkCmdEndRendering(commandBuffer);
    }
}
