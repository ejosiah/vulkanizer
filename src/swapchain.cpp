#include "vulkanizer/swapchain.hpp"

#include "vulkanizer/status.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace vkz {
    namespace {
        VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) {
            for (const auto& format : formats) {
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    return format;
                }
            }

            return formats.front();
        }

        VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& presentModes) {
            for (auto presentMode : presentModes) {
                if (presentMode == VK_PRESENT_MODE_FIFO_KHR) {
                    return presentMode;
                }
            }

            return VK_PRESENT_MODE_FIFO_KHR;
        }

        VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities) {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent;
            }

            return capabilities.minImageExtent;
        }

        uint32_t choose_image_count(const VkSurfaceCapabilitiesKHR& capabilities) {
            auto image_count = capabilities.minImageCount + 1;

            if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
                image_count = capabilities.maxImageCount;
            }

            return image_count;
        }
    }

    swapchain::swapchain(
            const context& context,
            VkSwapchainKHR swapchain,
            std::vector<VkImage> images,
            VkExtent2D extent,
            VkFormat format)
        : context_{context}
        , swapchain_{swapchain}
        , images_{std::move(images)}
        , format_{format}
        , width_{extent.width}
        , height_{extent.height} {
    }

    swapchain::~swapchain() {
        if (swapchain_) {
            vkDestroySwapchainKHR(context_.device.logical, swapchain_, nullptr);
        }
    }

    swapchain::swapchain(swapchain&& other) noexcept
        : context_{other.context_}
        , swapchain_{std::exchange(other.swapchain_, nullptr)}
        , images_{std::move(other.images_)}
        , format_{other.format_}
        , width_{other.width_}
        , height_{other.height_} {
    }

    swapchain::builder_base swapchain::builder(const context& context) {
        return swapchain::builder_base{context};
    }

    uint32_t swapchain::image_count() const {
        return VKZ_COUNT(images_);
    }

    VkImage swapchain::get_image(uint32_t index) {
        return images_.at(index);
    }

    VkFormat swapchain::format() const {
        return format_;
    }

    uint32_t swapchain::width() const {
        return width_;
    }

    uint32_t swapchain::height() const {
        return height_;
    }

    VkSwapchainKHR swapchain::handle() const {
        return swapchain_;
    }

    swapchain::operator VkSwapchainKHR() const {
        return handle();
    }

    swapchain::builder_base::builder_base(const context& context)
        : context_{context} {
        VkSurfaceCapabilitiesKHR capabilities{};
        VKZ_CHECK_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.device.physical, context_.surface, &capabilities));

        uint32_t formatCount{};
        VKZ_CHECK_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(context_.device.physical, context_.surface, &formatCount, nullptr));
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        VKZ_CHECK_VULKAN(vkGetPhysicalDeviceSurfaceFormatsKHR(context_.device.physical, context_.surface, &formatCount, formats.data()));

        uint32_t presentModeCount{};
        VKZ_CHECK_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(context_.device.physical, context_.surface, &presentModeCount, nullptr));
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        VKZ_CHECK_VULKAN(vkGetPhysicalDeviceSurfacePresentModesKHR(
                context_.device.physical,
                context_.surface,
                &presentModeCount,
                presentModes.data()));

        const auto surfaceFormat = choose_surface_format(formats);
        format_ = surfaceFormat.format;
        extent_ = choose_extent(capabilities);

        create_info_.surface = context_.surface;
        create_info_.minImageCount = choose_image_count(capabilities);
        create_info_.imageFormat = surfaceFormat.format;
        create_info_.imageColorSpace = surfaceFormat.colorSpace;
        create_info_.imageExtent = extent_;
        create_info_.imageArrayLayers = 1;
        create_info_.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        create_info_.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info_.preTransform = capabilities.currentTransform;
        create_info_.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info_.presentMode = choose_present_mode(presentModes);
        create_info_.clipped = VK_TRUE;
    }

    swapchain::builder_base& swapchain::builder_base::set_min_image_count(uint32_t value) {
        create_info_.minImageCount = value;
        return *this;
    }

    swapchain::builder_base& swapchain::builder_base::set_image_format(VkFormat format, VkColorSpaceKHR colorSpace) {
        format_ = format;
        create_info_.imageFormat = format;
        create_info_.imageColorSpace = colorSpace;
        return *this;
    }

    swapchain::builder_base& swapchain::builder_base::set_image_usage(VkImageUsageFlags usage) {
        create_info_.imageUsage = usage;
        return *this;
    }

    swapchain::builder_base& swapchain::builder_base::set_extent(uint32_t width, uint32_t height) {
        extent_ = {width, height};
        create_info_.imageExtent = extent_;
        return *this;
    }

    swapchain::builder_base& swapchain::builder_base::set_pre_transform(VkSurfaceTransformFlagBitsKHR transform) {
        create_info_.preTransform = transform;
        return *this;
    }

    swapchain::builder_base& swapchain::builder_base::set_composite_alpha(VkCompositeAlphaFlagBitsKHR compositeAlpha) {
        create_info_.compositeAlpha = compositeAlpha;
        return *this;
    }

    swapchain::builder_base& swapchain::builder_base::set_present_mode(VkPresentModeKHR mode) {
        create_info_.presentMode = mode;
        return *this;
    }

    swapchain swapchain::builder_base::build() {
        VkSwapchainKHR handle{};
        VKZ_CHECK_VULKAN(vkCreateSwapchainKHR(context_.device.logical, &create_info_, nullptr, &handle));

        uint32_t image_count{};
        VKZ_CHECK_VULKAN(vkGetSwapchainImagesKHR(context_.device.logical, handle, &image_count, nullptr));

        std::vector<VkImage> images(image_count);
        VKZ_CHECK_VULKAN(vkGetSwapchainImagesKHR(context_.device.logical, handle, &image_count, images.data()));

        return swapchain{context_, handle, std::move(images), extent_, format_};
    }
}
