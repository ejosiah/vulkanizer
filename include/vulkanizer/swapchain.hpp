#pragma once

#include "context.hpp"

#include <vector>

namespace vkz {

    class swapchain {
    public:
        class Builder;

        ~swapchain();

        swapchain(const swapchain&) = delete;

        swapchain& operator=(const swapchain&) = delete;

        swapchain(swapchain&& other) noexcept;

        swapchain& operator=(swapchain&& other) = delete;

        static Builder builder(const context& context);

        [[nodiscard]] uint32_t imageCount() const;

        VkImage getImage(uint32_t index);

        [[nodiscard]] VkFormat format() const;

        [[nodiscard]] uint32_t width() const;

        [[nodiscard]] uint32_t  height() const;

        [[nodiscard]] VkSwapchainKHR handle() const;

        operator VkSwapchainKHR() const;

    private:
        friend class Builder;

        swapchain(const context& context, VkSwapchainKHR swapchain, std::vector<VkImage> images, VkExtent2D extent, VkFormat format);

        const context& context_;
        VkSwapchainKHR swapchain_;
        std::vector<VkImage> images_;
        VkFormat format_{};
        uint32_t width_{};
        uint32_t height_{};
    };

    class swapchain::Builder {
    public:
        explicit Builder(const context& context);

        [[maybe_unused]] Builder& setMinImageCount(uint32_t value);

        [[maybe_unused]] Builder& setImageFormat(VkFormat format, VkColorSpaceKHR colorSpace);

        [[maybe_unused]] Builder& setExtent(uint32_t width, uint32_t height);

        [[maybe_unused]] Builder& setPreTransform(VkSurfaceTransformFlagBitsKHR transform);

        [[maybe_unused]] Builder& setCompositeAlpha(VkCompositeAlphaFlagBitsKHR compositeAlpha);

        [[maybe_unused]] Builder& setPresentMode(VkPresentModeKHR mode);

        swapchain build();

    private:
        const context& context_;
        VkSwapchainCreateInfoKHR createInfo_{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        VkFormat format_{};
        VkExtent2D extent_{};
    };

}
