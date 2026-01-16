#pragma once

#include "status.hpp"

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <format>
#include <type_traits>

namespace vkz {
    template<typename T>
    inline T makeStruct() {
        VKZ_THROW(std::format("makeStruct not implemented for type: {}", typeid(T).name()));
    }

    template<>
    inline VkBufferCreateInfo makeStruct<VkBufferCreateInfo>() {
        return { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    }

    template<>
    inline VkImageCreateInfo makeStruct<VkImageCreateInfo>() {
        return { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    }

    template<>
    inline VkCommandBufferAllocateInfo makeStruct<VkCommandBufferAllocateInfo>() {
        return { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    }

    template<>
    inline VkCommandBufferBeginInfo makeStruct<VkCommandBufferBeginInfo>() {
        return { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    }

    template<>
    inline VkSubmitInfo makeStruct<VkSubmitInfo>() {
        return { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    }

    template<>
    inline VkBufferMemoryBarrier makeStruct<VkBufferMemoryBarrier>() {
        return { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    }

    template<>
    inline VkMemoryBarrier makeStruct<VkMemoryBarrier>() {
        return { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    }

    template<>
    inline VkImageMemoryBarrier makeStruct<VkImageMemoryBarrier>() {
        return { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    }

    template<>
    inline VkRenderingInfo makeStruct<VkRenderingInfo>() {
        return { VK_STRUCTURE_TYPE_RENDERING_INFO };
    }

    template<>
    inline VkRenderingAttachmentInfo makeStruct<VkRenderingAttachmentInfo>() {
        return { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    }

    template<>
    inline VkImageViewCreateInfo makeStruct<VkImageViewCreateInfo>() {
        return { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    }

    template<>
    inline VkPhysicalDeviceDynamicRenderingFeatures makeStruct<VkPhysicalDeviceDynamicRenderingFeatures>() {
        return { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
    }

    template<>
    inline VkDeviceCreateInfo makeStruct<VkDeviceCreateInfo>() {
        return { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    }

    template<>
    inline VkDescriptorPoolCreateInfo makeStruct<VkDescriptorPoolCreateInfo>() {
        return { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    }

    template<>
    inline VkDescriptorSetLayoutCreateInfo makeStruct<VkDescriptorSetLayoutCreateInfo>() {
        return { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    }

    template<>
    inline VkDescriptorSetAllocateInfo makeStruct<VkDescriptorSetAllocateInfo>() {
        return { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    }

    template<>
    inline VkWriteDescriptorSet makeStruct<VkWriteDescriptorSet>() {
        return { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    }

    template<>
    inline VkPipelineLayoutCreateInfo makeStruct<VkPipelineLayoutCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    }

    template<>
    inline VkGraphicsPipelineCreateInfo makeStruct<VkGraphicsPipelineCreateInfo>() {
        return { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    }


    template<>
    inline VkComputePipelineCreateInfo makeStruct<VkComputePipelineCreateInfo>() {
        return { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    }

    template<>
    inline VkShaderModuleCreateInfo makeStruct<VkShaderModuleCreateInfo>() {
        return { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    }

    template<>
    inline VkPipelineShaderStageCreateInfo makeStruct<VkPipelineShaderStageCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    }

    template<>
    inline VkPipelineVertexInputStateCreateInfo makeStruct<VkPipelineVertexInputStateCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    }

    template<>
    inline VkPipelineInputAssemblyStateCreateInfo makeStruct<VkPipelineInputAssemblyStateCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    }

    template<>
    inline VkPipelineTessellationStateCreateInfo makeStruct<VkPipelineTessellationStateCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
    }

    template<>
    inline VkPipelineViewportStateCreateInfo makeStruct<VkPipelineViewportStateCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    }

    template<>
    inline VkPipelineRasterizationStateCreateInfo makeStruct<VkPipelineRasterizationStateCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    }

    template<>
    inline VkPipelineMultisampleStateCreateInfo makeStruct<VkPipelineMultisampleStateCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    }

    template<>
    inline VkPipelineDepthStencilStateCreateInfo makeStruct<VkPipelineDepthStencilStateCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    }

    template<>
    inline VkPipelineColorBlendStateCreateInfo makeStruct<VkPipelineColorBlendStateCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    }

    template<>
    inline VkPipelineDynamicStateCreateInfo makeStruct<VkPipelineDynamicStateCreateInfo>() {
        return { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    }

    template<>
    inline VkRenderPassCreateInfo makeStruct<VkRenderPassCreateInfo>() {
        return { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    }

    template<>
    inline VkFramebufferCreateInfo makeStruct<VkFramebufferCreateInfo>() {
        return { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    }

    template<>
    inline VkRenderPassBeginInfo makeStruct<VkRenderPassBeginInfo>() {
        return { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    }

    template<>
    inline VkSwapchainCreateInfoKHR makeStruct<VkSwapchainCreateInfoKHR>() {
        return { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    }

    template<>
    inline VkPresentInfoKHR makeStruct<VkPresentInfoKHR>() {
        return { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    }

    template<>
    inline VkSemaphoreCreateInfo makeStruct<VkSemaphoreCreateInfo>() {
        return { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    }

    template<>
    inline VkFenceCreateInfo makeStruct<VkFenceCreateInfo>() {
        return { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    }

    template<>
    inline VkDebugUtilsObjectNameInfoEXT makeStruct<VkDebugUtilsObjectNameInfoEXT>() {
        return { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    }

}