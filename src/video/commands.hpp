#pragma once

#include <vulkanizer/vkz.hpp>
#include <span>

namespace vkz::video {
    // Vulkan Video commands have no core graphics/compute equivalents.
    inline void begin_coding(VkCommandBuffer command, VkVideoSessionKHR session, VkVideoSessionParametersKHR parameters, std::span<const VkVideoReferenceSlotInfoKHR> slots) {
        VkVideoBeginCodingInfoKHR info{VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR};
        info.videoSession = session;
        info.videoSessionParameters = parameters;
        info.referenceSlotCount = static_cast<uint32_t>(slots.size());
        info.pReferenceSlots = slots.data();
        vkCmdBeginVideoCodingKHR(command, &info);
    }

    inline void reset_coding(VkCommandBuffer command) {
        VkVideoCodingControlInfoKHR info{VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR};
        info.flags = static_cast<VkVideoCodingControlFlagsKHR>(vkz::video_coding_control::reset);
        vkCmdControlVideoCodingKHR(command, &info);
    }

    inline void decode_picture(VkCommandBuffer command, const VkVideoDecodeInfoKHR &info) {
        vkCmdDecodeVideoKHR(command, &info);
    }

    inline void end_coding(VkCommandBuffer command) {
        VkVideoEndCodingInfoKHR info{VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR};
        vkCmdEndVideoCodingKHR(command, &info);
    }
} // namespace vkz::video
