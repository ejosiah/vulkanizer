#include "vulkanizer/swapchain.hpp"

#include "vulkanizer/status.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace vkz {
    namespace {
        VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
            for (const auto& format : formats) {
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    return format;
                }
            }

            return formats.front();
        }

        VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) {
            for (auto presentMode : presentModes) {
                if (presentMode == VK_PRESENT_MODE_FIFO_KHR) {
                    return presentMode;
                }
            }

            return VK_PRESENT_MODE_FIFO_KHR;
        }

        VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent;
            }

            return capabilities.minImageExtent;
        }

        uint32_t chooseImageCount(const VkSurfaceCapabilitiesKHR& capabilities) {
            auto imageCount = capabilities.minImageCount + 1;

            if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
                imageCount = capabilities.maxImageCount;
            }

            return imageCount;
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

    swapchain::Builder swapchain::builder(const context& context) {
        return swapchain::Builder{context};
    }

    uint32_t swapchain::imageCount() const {
        return VKZ_COUNT(images_);
    }

    VkImage swapchain::getImage(uint32_t index) {
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

    swapchain::Builder::Builder(const context& context)
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

        const auto surfaceFormat = chooseSurfaceFormat(formats);
        format_ = surfaceFormat.format;
        extent_ = chooseExtent(capabilities);

        createInfo_.surface = context_.surface;
        createInfo_.minImageCount = chooseImageCount(capabilities);
        createInfo_.imageFormat = surfaceFormat.format;
        createInfo_.imageColorSpace = surfaceFormat.colorSpace;
        createInfo_.imageExtent = extent_;
        createInfo_.imageArrayLayers = 1;
        createInfo_.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        createInfo_.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo_.preTransform = capabilities.currentTransform;
        createInfo_.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo_.presentMode = choosePresentMode(presentModes);
        createInfo_.clipped = VK_TRUE;
    }

    swapchain::Builder& swapchain::Builder::setMinImageCount(uint32_t value) {
        createInfo_.minImageCount = value;
        return *this;
    }

    swapchain::Builder& swapchain::Builder::setImageFormat(VkFormat format, VkColorSpaceKHR colorSpace) {
        format_ = format;
        createInfo_.imageFormat = format;
        createInfo_.imageColorSpace = colorSpace;
        return *this;
    }

    swapchain::Builder& swapchain::Builder::setImageUsage(VkImageUsageFlags usage) {
        createInfo_.imageUsage = usage;
        return *this;
    }

    swapchain::Builder& swapchain::Builder::setExtent(uint32_t width, uint32_t height) {
        extent_ = {width, height};
        createInfo_.imageExtent = extent_;
        return *this;
    }

    swapchain::Builder& swapchain::Builder::setPreTransform(VkSurfaceTransformFlagBitsKHR transform) {
        createInfo_.preTransform = transform;
        return *this;
    }

    swapchain::Builder& swapchain::Builder::setCompositeAlpha(VkCompositeAlphaFlagBitsKHR compositeAlpha) {
        createInfo_.compositeAlpha = compositeAlpha;
        return *this;
    }

    swapchain::Builder& swapchain::Builder::setPresentMode(VkPresentModeKHR mode) {
        createInfo_.presentMode = mode;
        return *this;
    }

    swapchain swapchain::Builder::build() {
        VkSwapchainKHR handle{};
        VKZ_CHECK_VULKAN(vkCreateSwapchainKHR(context_.device.logical, &createInfo_, nullptr, &handle));

        uint32_t imageCount{};
        VKZ_CHECK_VULKAN(vkGetSwapchainImagesKHR(context_.device.logical, handle, &imageCount, nullptr));

        std::vector<VkImage> images(imageCount);
        VKZ_CHECK_VULKAN(vkGetSwapchainImagesKHR(context_.device.logical, handle, &imageCount, images.data()));

        return swapchain{context_, handle, std::move(images), extent_, format_};
    }
}
