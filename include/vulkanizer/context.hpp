#pragma once

#include "vkz.hpp"
#include "surface_provider.hpp"
#include "types.hpp"
#include <cstring>
#include <new>
#include <string>
#include <type_traits>
#include <concepts>

namespace vkz {

    class builder;

    template <typename T>
    concept VulkanStructure =
        requires(T t) {
            { t.sType } -> std::convertible_to<VkStructureType>;
            { t.pNext } -> std::convertible_to<const void*>;
        };

    struct context {
        context() = default;

        ~context();

        context(const context&) = delete;

        context& operator=(const context&) = delete;

        context(context&& other) noexcept;

        context& operator=(context&& other) noexcept;

        VkInstance instance{};
        VkDebugUtilsMessengerEXT debugMessenger{};
        VkSurfaceKHR surface{};
        Device device{};

        static vkz::builder builder();
    };

    class builder {
    public:
        builder();

        ~builder();

        builder(const builder&) = delete;

        builder& operator=(const builder&) = delete;

        builder(builder&& other) noexcept;

        builder& operator=(builder&& other) noexcept;

        builder& appName(const std::string& appName);

        builder& appVersion(const std::string& appVersion);

        builder& engineName(const std::string& engineName);

        builder& engineVersion(const std::string& engineVersion);

        builder& apiVersion(uint apiVersion);

        builder& addInstanceExtension(const std::string& extension);

        builder& addInstanceLayer(const std::string& layer);

        builder& addDeviceExtension(const std::string& extension);

        builder& addDeviceLayer(const std::string& layer);

        builder& enabledFeatures(const VkPhysicalDeviceFeatures& features);

        builder& surface(const surface_provider& surface);

        builder& addQueue(VkQueueFlagBits flag);

        builder& addUniqueQueue(VkQueueFlagBits flag);

        builder& numGraphicsQueues(uint count);

        template <VulkanStructure T>
        builder& addExtension(const T& extension) {
            static_assert(std::is_trivially_copyable_v<T>);
            auto* next = static_cast<T*>(::operator new(sizeof(T)));
            std::memcpy(next, &extension, sizeof(T));

            auto* node = reinterpret_cast<VkBaseOutStructure*>(next);
            node->pNext = _extensions;
            _extensions = node;

            return *this;
        }

        context build();

    private:
        class Impl;

        VkBaseOutStructure* _extensions{};
        Impl* pimpl;
    };
}
