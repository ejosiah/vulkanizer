#pragma once

#include "context.hpp"

#include <vector>

namespace vkz {

    class swapchain {
    public:
        class builder_base;

        ~swapchain();

        swapchain(const swapchain&) = delete;

        swapchain& operator=(const swapchain&) = delete;

        swapchain(swapchain&& other) noexcept;

        swapchain& operator=(swapchain&& other) = delete;

        static builder_base builder(const context& context);

        [[nodiscard]] uint32_t image_count() const;

        VkImage get_image(uint32_t index);

        [[nodiscard]] VkFormat format() const;

        [[nodiscard]] uint32_t width() const;

        [[nodiscard]] uint32_t  height() const;

        [[nodiscard]] VkSwapchainKHR handle() const;

        operator VkSwapchainKHR() const;

    private:
        friend class builder_base;

        swapchain(const context& context, VkSwapchainKHR swapchain, std::vector<VkImage> images, VkExtent2D extent, VkFormat format);

        const context& context_;
        VkSwapchainKHR swapchain_;
        std::vector<VkImage> images_;
        VkFormat format_{};
        uint32_t width_{};
        uint32_t height_{};
    };

    class swapchain::builder_base {
    public:
        explicit builder_base(const context& context);

        [[maybe_unused]] builder_base& set_min_image_count(uint32_t value);

        [[maybe_unused]] builder_base& set_image_format(VkFormat format, VkColorSpaceKHR colorSpace);

        [[maybe_unused]] builder_base& set_image_usage(VkImageUsageFlags usage);

        [[maybe_unused]] builder_base& set_extent(uint32_t width, uint32_t height);

        [[maybe_unused]] builder_base& set_pre_transform(VkSurfaceTransformFlagBitsKHR transform);

        [[maybe_unused]] builder_base& set_composite_alpha(VkCompositeAlphaFlagBitsKHR compositeAlpha);

        [[maybe_unused]] builder_base& set_present_mode(VkPresentModeKHR mode);

        swapchain build();

    private:
        const context& context_;
        VkSwapchainCreateInfoKHR create_info_{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        VkFormat format_{};
        VkExtent2D extent_{};
    };

}
