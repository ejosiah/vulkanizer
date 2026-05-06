#define VKZ_IOSTREAM_ADAPTOR

#include <vulkanizer/context.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/swapchain.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace {
    constexpr uint32_t WindowWidth = 800;
    constexpr uint32_t WindowHeight = 600;

    class glfw_surface_provider final : public vkz::surface_provider {
    public:
        explicit glfw_surface_provider(GLFWwindow* window)
            : _window{window} {
        }

        VkSurfaceKHR operator()(VkInstance instance) const override {
            VkSurfaceKHR surface{};
            VKZ_CHECK_VULKAN(glfwCreateWindowSurface(instance, _window, nullptr, &surface));
            return surface;
        }

    private:
        GLFWwindow* _window{};
    };

    uint32_t findContextGraphicsPresentQueueFamily(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        uint32_t queueFamilyCount{};
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            if (!(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                continue;
            }

            VkBool32 presentSupported{};
            VKZ_CHECK_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupported));

            if (!presentSupported) {
                VKZ_THROW("The context graphics queue family does not support presentation")
            }

            return i;
        }

        VKZ_THROW("No graphics queue family is available")
    }

    VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool) {
        VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer{};
        VKZ_CHECK_VULKAN(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKZ_CHECK_VULKAN(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        return commandBuffer;
    }

    void transitionImage(
            VkCommandBuffer commandBuffer,
            VkImage image,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            VkAccessFlags srcAccessMask,
            VkAccessFlags dstAccessMask,
            VkPipelineStageFlags srcStage,
            VkPipelineStageFlags dstStage) {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;

        vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void submitAndFree(
            VkDevice device,
            VkQueue queue,
            VkCommandPool commandPool,
            VkCommandBuffer commandBuffer,
            VkSemaphore waitSemaphore,
            VkSemaphore signalSemaphore,
            VkFence fence) {
        VKZ_CHECK_VULKAN(vkEndCommandBuffer(commandBuffer));

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSemaphore;

        VKZ_CHECK_VULKAN(vkQueueSubmit(queue, 1, &submitInfo, fence));
        VKZ_CHECK_VULKAN(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
        VKZ_CHECK_VULKAN(vkResetFences(device, 1, &fence));

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
}

int main() {
    vkz::iostream_adapter::install(std::cout);

    if (!glfwInit()) {
        VKZ_THROW("Failed to initialize GLFW")
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WindowWidth, WindowHeight, "vulkanizer context test", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        VKZ_THROW("Failed to create GLFW window")
    }

    uint32_t requiredExtensionCount{};
    const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
    if (!requiredExtensions) {
        glfwDestroyWindow(window);
        glfwTerminate();
        VKZ_THROW("GLFW could not provide Vulkan instance extensions")
    }

    glfw_surface_provider surfaceProvider{window};

    auto builder = vkz::context::builder();
    builder
            .appName("vulkanizer context test")
            .engineName("vulkanizer")
            .surface(surfaceProvider)
            .addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    for (uint32_t i = 0; i < requiredExtensionCount; ++i) {
        builder.addInstanceExtension(requiredExtensions[i]);
    }

    auto context = builder.build();
    const auto surface = context.surface;

    const auto queueFamilyIndex = findContextGraphicsPresentQueueFamily(context.device.physical, surface);
    VkQueue graphicsQueue{};
    vkGetDeviceQueue(context.device.logical, queueFamilyIndex, 0, &graphicsQueue);

    {
    auto swapchain = vkz::swapchain::builder(context).build();

    VkCommandPoolCreateInfo commandPoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool commandPool{};
    VKZ_CHECK_VULKAN(vkCreateCommandPool(context.device.logical, &commandPoolCreateInfo, nullptr, &commandPool));

    VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore imageAvailable{};
    VkSemaphore renderFinished{};
    VKZ_CHECK_VULKAN(vkCreateSemaphore(context.device.logical, &semaphoreCreateInfo, nullptr, &imageAvailable));
    VKZ_CHECK_VULKAN(vkCreateSemaphore(context.device.logical, &semaphoreCreateInfo, nullptr, &renderFinished));

    VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence frameFence{};
    VKZ_CHECK_VULKAN(vkCreateFence(context.device.logical, &fenceCreateInfo, nullptr, &frameFence));

    const std::array<VkClearColorValue, 3> colors{
            VkClearColorValue{{1.0f, 0.0f, 0.0f, 1.0f}},
            VkClearColorValue{{0.0f, 0.0f, 1.0f, 1.0f}},
            VkClearColorValue{{0.0f, 1.0f, 0.0f, 1.0f}},
    };

    constexpr uint32_t MaxFrames = 18;
    for (uint32_t frame = 0; frame < MaxFrames && !glfwWindowShouldClose(window); ++frame) {
        glfwPollEvents();

        uint32_t imageIndex{};
        const auto acquireResult = vkAcquireNextImageKHR(
                context.device.logical,
                swapchain.handle(),
                UINT64_MAX,
                imageAvailable,
                {},
                &imageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            break;
        }
        VKZ_CHECK_VULKAN(acquireResult);

        auto commandBuffer = beginCommandBuffer(context.device.logical, commandPool);
        transitionImage(
                commandBuffer,
                swapchain.getImage(imageIndex),
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1;
        range.layerCount = 1;
        vkCmdClearColorImage(commandBuffer, swapchain.getImage(imageIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &colors[frame % colors.size()], 1, &range);

        transitionImage(
                commandBuffer,
                swapchain.getImage(imageIndex),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                0,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        submitAndFree(context.device.logical, graphicsQueue, commandPool, commandBuffer, imageAvailable, renderFinished,
                      frameFence);

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        const auto swapchainHandle = swapchain.handle();
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinished;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchainHandle;
        presentInfo.pImageIndices = &imageIndex;

        const auto presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            break;
        }
        VKZ_CHECK_VULKAN(presentResult);

        std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }

    vkDeviceWaitIdle(context.device.logical);

    vkDestroyFence(context.device.logical, frameFence, nullptr);
    vkDestroySemaphore(context.device.logical, renderFinished, nullptr);
    vkDestroySemaphore(context.device.logical, imageAvailable, nullptr);
    vkDestroyCommandPool(context.device.logical, commandPool, nullptr);
    }

    context = {};

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
