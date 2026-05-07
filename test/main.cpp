#define VKZ_IOSTREAM_ADAPTOR

#include <vulkanizer/barrier.hpp>
#include <vulkanizer/context.hpp>
#include <vulkanizer/imgui.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/render.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/swapchain.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <iostream>
#include <memory>
#include <vector>

namespace {
    constexpr uint32_t WindowWidth = 1280;
    constexpr uint32_t WindowHeight = 800;

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

    void submitAndFree(
            VkDevice device,
            VkQueue queue,
            VkCommandPool commandPool,
            VkCommandBuffer commandBuffer,
            VkSemaphore waitSemaphore,
            VkSemaphore signalSemaphore,
            VkFence fence) {
        VKZ_CHECK_VULKAN(vkEndCommandBuffer(commandBuffer));

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

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

    std::vector<vkz::ImageView> createImageViews(VkDevice device, vkz::swapchain& swapchain) {
        std::vector<vkz::ImageView> imageViews;
        imageViews.reserve(swapchain.imageCount());

        for (uint32_t i = 0; i < swapchain.imageCount(); ++i) {
            VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            createInfo.image = swapchain.getImage(i);
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapchain.format();
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.layerCount = 1;

            VkImageView imageView{};
            VKZ_CHECK_VULKAN(vkCreateImageView(device, &createInfo, nullptr, &imageView));
            imageViews.push_back({
                    .handle = imageView,
                    .info = createInfo,
            });
        }

        return imageViews;
    }

    void destroyImageViews(VkDevice device, std::vector<vkz::ImageView>& imageViews) {
        for (auto imageView : imageViews) {
            vkDestroyImageView(device, imageView.handle, nullptr);
        }

        imageViews.clear();
    }

    void waitForDrawableWindow(GLFWwindow* window) {
        int width{};
        int height{};
        glfwGetFramebufferSize(window, &width, &height);

        while ((width == 0 || height == 0) && !glfwWindowShouldClose(window)) {
            glfwWaitEvents();
            glfwGetFramebufferSize(window, &width, &height);
        }
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
    VkPhysicalDeviceSynchronization2Features synchronization2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            nullptr,
            VK_TRUE,
    };
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            nullptr,
            VK_TRUE,
    };

    auto builder = vkz::context::builder();
    builder
            .appName("vulkanizer context test")
            .engineName("vulkanizer")
            .apiVersion(VK_API_VERSION_1_3)
            .surface(surfaceProvider)
            .addExtension(synchronization2)
            .addExtension(dynamicRendering)
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
    auto createSwapchain = [&context] {
        return std::make_unique<vkz::swapchain>(
                vkz::swapchain::builder(context)
                        .setImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                        .build());
    };

    auto swapchain = createSwapchain();
    auto imageViews = createImageViews(context.device.logical, *swapchain);

    vkz::imgui::init({
            .window = window,
            .vulkanContext = &context,
            .queueFamily = queueFamilyIndex,
            .queue = graphicsQueue,
            .minImageCount = 2,
            .imageCount = swapchain->imageCount(),
            .apiVersion = VK_API_VERSION_1_3,
            .colorAttachmentFormat = swapchain->format(),
    });

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

    auto recreateSwapchain = [&] {
        vkDeviceWaitIdle(context.device.logical);
        waitForDrawableWindow(window);
        if (glfwWindowShouldClose(window)) {
            return;
        }

        destroyImageViews(context.device.logical, imageViews);
        swapchain.reset();
        swapchain = createSwapchain();
        imageViews = createImageViews(context.device.logical, *swapchain);
    };

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        waitForDrawableWindow(window);
        if (glfwWindowShouldClose(window)) {
            break;
        }

        int framebufferWidth{};
        int framebufferHeight{};
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (static_cast<uint32_t>(framebufferWidth) != swapchain->width() ||
            static_cast<uint32_t>(framebufferHeight) != swapchain->height()) {
            recreateSwapchain();
            continue;
        }

        uint32_t imageIndex{};
        const auto acquireResult = vkAcquireNextImageKHR(
                context.device.logical,
                swapchain->handle(),
                UINT64_MAX,
                imageAvailable,
                {},
                &imageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            continue;
        }
        VKZ_CHECK_VULKAN(acquireResult);

        vkz::imgui::newFrame();
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }
        auto commandBuffer = beginCommandBuffer(context.device.logical, commandPool);
        auto image = swapchain->getImage(imageIndex);
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1;
        range.layerCount = 1;

        vkz::barrier::pushAndFlush(
                commandBuffer,
                image,
                range,
                VK_PIPELINE_STAGE_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_NONE,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        vkz::render_info renderInfo{};
        renderInfo.colorAttachments.push_back({
                .imageView = imageViews[imageIndex],
                .format = swapchain->format(),
                .clearValue = {clear_color.x, clear_color.y, clear_color.z, clear_color.w},
        });
        renderInfo.renderArea = {swapchain->width(), swapchain->height()};

        vkz::render(commandBuffer, renderInfo, [&] {
            vkz::imgui::render(commandBuffer);
        });

        vkz::barrier::pushAndFlush(
                commandBuffer,
                image,
                range,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_2_NONE,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        submitAndFree(context.device.logical, graphicsQueue, commandPool, commandBuffer, imageAvailable, renderFinished,
                      frameFence);

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        const auto swapchainHandle = swapchain->handle();
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinished;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchainHandle;
        presentInfo.pImageIndices = &imageIndex;

        const auto presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain();
            continue;
        }
        VKZ_CHECK_VULKAN(presentResult);
    }

    vkDeviceWaitIdle(context.device.logical);

    vkz::imgui::destroy();

    destroyImageViews(context.device.logical, imageViews);
    swapchain.reset();

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
