#pragma once

#include "vkz.hpp"
#include "surface_provider.hpp"
#include "types.hpp"
#include "device_extension_chain.hpp"

#include <concepts>
#include <span>
#include <string>
#include <tuple>

namespace vkz {

    class builder;

    struct context {
        context() = default;

        ~context();

        context(const context&) = delete;

        context& operator=(const context&) = delete;

        context(context&& other) noexcept;

        context& operator=(context&& other) noexcept;

        VkInstance instance{};
        VkDebugUtilsMessengerEXT debug_messenger{};
        VkSurfaceKHR surface{};
        device device{};
        uint32_t api_version{VK_API_VERSION_1_3};

        static builder builder();

        static context create_not_owned(
            VkInstance instance,
            vkz::device device,
            VkSurfaceKHR surface = {},
            VkDebugUtilsMessengerEXT debug_messenger = {},
            uint32_t api_version = VK_API_VERSION_1_3);

    private:
        bool owns_components_{true};
    };

    class device_factory {
    public:
        virtual ~device_factory() = default;

        virtual VkInstance create_instance() = 0;

        virtual std::tuple<VkPhysicalDevice, VkDevice> create_device() = 0;

        void set_builder(const vkz::builder& builder) {
            builder_ = &builder;
        }

    protected:
        [[nodiscard]] VkQueueFlags queue_flags() const;

        [[nodiscard]] VkQueueFlags unique_queue_flags() const;

        [[nodiscard]] uint32_t graphics_queue_count() const;

        [[nodiscard]] std::span<const char* const> instance_extensions() const;

        [[nodiscard]] std::span<const char* const> instance_layers() const;

        [[nodiscard]] std::span<const char* const> device_extensions() const;

        [[nodiscard]] std::span<const char* const> device_layers() const;

        [[nodiscard]] const VkPhysicalDeviceFeatures& enabled_features() const;

        [[nodiscard]] const void* device_extension_chain() const;

    private:
        const vkz::builder* builder_{};
    };

    class builder {
    public:
        builder();

        ~builder();

        builder(const builder&) = delete;

        builder& operator=(const builder&) = delete;

        builder(builder&& other) noexcept;

        builder& operator=(builder&& other) noexcept;

        builder& app_name(const std::string& app_name);

        builder& app_version(const std::string& app_version);

        builder& engine_name(const std::string& engine_name);

        builder& engine_version(const std::string& engine_version);

        builder& api_version(uint api_version);

        builder& add_instance_extension(const std::string& extension);

        builder& add_instance_layer(const std::string& layer);

        builder& add_device_extension(const std::string& extension);

        builder& add_device_layer(const std::string& layer);

        builder& enabled_features(const VkPhysicalDeviceFeatures& features);

        builder& surface(const surface_provider& surface);

        builder& device_factory(vkz::device_factory& factory);

        builder& add_queue(VkQueueFlagBits flag);

        builder& add_unique_queue(VkQueueFlagBits flag);

        builder& num_graphics_queues(uint count);

        [[nodiscard]] VkQueueFlags queue_flags() const;

        [[nodiscard]] VkQueueFlags unique_queue_flags() const;

        [[nodiscard]] uint32_t graphics_queue_count() const;

        builder& add_extension_chain(const device_extension_chain& extensions);

        template <vulkan_structure T>
        builder& add_extension(const T& extension) {
            _extensions.add(extension);
            return *this;
        }

        [[nodiscard]] context build();

    private:
        friend class default_factory;
        friend class device_factory;
        class Impl;

        device_extension_chain _extensions;
        Impl* pimpl;
    };
}
