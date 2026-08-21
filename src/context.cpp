#include "vulkanizer/context.hpp"

#include "vulkanizer/aftermath.hpp"
#include "vulkanizer/log.hpp"
#include "vulkanizer/status.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vkz {
    namespace {
        VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
                VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                VkDebugUtilsMessageTypeFlagsEXT,
                const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                void*) {
            const auto message = callbackData && callbackData->pMessage
                                 ? std::string_view{callbackData->pMessage}
                                 : std::string_view{};

            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                error(message);
            } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                warn(message);
            } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
                info(message);
            } else {
                debug(message);
            }

            return VK_FALSE;
        }

        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info() {
            VkDebugUtilsMessengerCreateInfoEXT create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            create_info.messageSeverity =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            create_info.messageType =
                    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            create_info.pfnUserCallback = debug_callback;
            return create_info;
        }

        uint32_t parse_version(const std::string& value) {
            if (value.empty()) {
                return VK_MAKE_API_VERSION(0, 0, 0, 0);
            }

            uint32_t parts[3]{};
            uint32_t partIndex = 0;

            for (char ch : value) {
                if (std::isdigit(static_cast<unsigned char>(ch))) {
                    parts[partIndex] = parts[partIndex] * 10u + static_cast<uint32_t>(ch - '0');
                } else if (ch == '.' && partIndex < 2) {
                    ++partIndex;
                } else {
                    break;
                }
            }

            return VK_MAKE_API_VERSION(0, parts[0], parts[1], parts[2]);
        }

        std::vector<VkQueueFlagBits> queue_bits(VkQueueFlags flags) {
            std::vector<VkQueueFlagBits> bits;

            const VkQueueFlagBits knownBits[] = {
                    VK_QUEUE_GRAPHICS_BIT,
                    VK_QUEUE_COMPUTE_BIT,
                    VK_QUEUE_TRANSFER_BIT,
                    VK_QUEUE_SPARSE_BINDING_BIT,
                    VK_QUEUE_PROTECTED_BIT,
#ifdef VK_QUEUE_VIDEO_DECODE_BIT_KHR
                    VK_QUEUE_VIDEO_DECODE_BIT_KHR,
#endif
#ifdef VK_QUEUE_VIDEO_ENCODE_BIT_KHR
                    VK_QUEUE_VIDEO_ENCODE_BIT_KHR,
#endif
            };

            for (auto bit : knownBits) {
                if (flags & bit) {
                    bits.push_back(bit);
                }
            }

            return bits;
        }

        bool contains(const std::vector<const char*>& values, const char* needle) {
            return std::ranges::any_of(values, [needle](const char* value) {
                return std::string_view{value} == needle;
            });
        }

        void log_extensions(std::string_view kind, const std::vector<const char*>& extensions) {
            std::string message{"Enabled "};
            message += kind;
            message += " extensions (";
            message += std::to_string(extensions.size());
            message += "):";

            if (extensions.empty()) {
                message += " none";
            } else {
                for (const auto* extension : extensions) {
                    message += "\n  - ";
                    message += extension;
                }
            }

            info(message);
        }

        bool supports_instance_layers(const std::vector<const char*>& layers) {
            uint32_t count{};
            VKZ_CHECK_VULKAN(vkEnumerateInstanceLayerProperties(&count, nullptr));

            std::vector<VkLayerProperties> available(count);
            VKZ_CHECK_VULKAN(vkEnumerateInstanceLayerProperties(&count, available.data()));

            return std::ranges::all_of(layers, [&available](const char* layer) {
                return std::ranges::any_of(available, [layer](const auto& properties) {
                    return std::string_view{properties.layerName} == layer;
                });
            });
        }

        bool supports_instance_extensions(const std::vector<const char*>& extensions) {
            uint32_t count{};
            VKZ_CHECK_VULKAN(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));

            std::vector<VkExtensionProperties> available(count);
            VKZ_CHECK_VULKAN(vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data()));

            return std::ranges::all_of(extensions, [&available](const char* extension) {
                return std::ranges::any_of(available, [extension](const auto& properties) {
                    return std::string_view{properties.extensionName} == extension;
                });
            });
        }

        bool supports_device_extensions(VkPhysicalDevice physical_device, const std::vector<const char*>& extensions) {
            uint32_t count{};
            VKZ_CHECK_VULKAN(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, nullptr));

            std::vector<VkExtensionProperties> available(count);
            VKZ_CHECK_VULKAN(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, available.data()));

            return std::ranges::all_of(extensions, [&available](const char* extension) {
                return std::ranges::any_of(available, [extension](const auto& properties) {
                    return std::string_view{properties.extensionName} == extension;
                });
            });
        }

        bool supports_device_layers(VkPhysicalDevice physical_device, const std::vector<const char*>& layers) {
            uint32_t count{};
            VKZ_CHECK_VULKAN(vkEnumerateDeviceLayerProperties(physical_device, &count, nullptr));

            std::vector<VkLayerProperties> available(count);
            VKZ_CHECK_VULKAN(vkEnumerateDeviceLayerProperties(physical_device, &count, available.data()));

            return std::ranges::all_of(layers, [&available](const char* layer) {
                return std::ranges::any_of(available, [layer](const auto& properties) {
                    return std::string_view{properties.layerName} == layer;
                });
            });
        }

        void destroy_context(context& context) {
            if (context.device.logical) {
                vkDeviceWaitIdle(context.device.logical);
                vkDestroyDevice(context.device.logical, nullptr);
                context.device.logical = nullptr;
            }

            if (context.surface) {
                vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
                context.surface = nullptr;
            }

#ifndef NDEBUG
            if (context.debug_messenger && vkDestroyDebugUtilsMessengerEXT) {
                vkDestroyDebugUtilsMessengerEXT(context.instance, context.debug_messenger, nullptr);
                context.debug_messenger = nullptr;
            }
#endif

            if (context.instance) {
                vkDestroyInstance(context.instance, nullptr);
                context.instance = nullptr;
            }

            context.device.physical = nullptr;
        }
    }

    class builder::Impl {
    public:
        std::string app_name{"vulkanizer"};
        std::string engine_name{"vulkanizer"};
        VkApplicationInfo application_info{
                VK_STRUCTURE_TYPE_APPLICATION_INFO,
                nullptr,
                app_name.c_str(),
                0,
                engine_name.c_str(),
                0,
                VK_API_VERSION_1_3,
        };

        std::vector<std::string> instance_extensions;
        std::vector<std::string> instance_validation_layers;
        std::vector<std::string> device_extensions;
        std::vector<std::string> device_validation_layers;

        std::vector<const char*> instance_extension_pointers;
        std::vector<const char*> instance_validation_layer_pointers;
        std::vector<const char*> device_extension_pointers;
        std::vector<const char*> device_validation_layer_pointers;

        VkPhysicalDeviceFeatures enabled_features{};
        const surface_provider* surface_provider{};
        VkSurfaceKHR surface{};

        VkQueueFlags queue_flags{VK_QUEUE_GRAPHICS_BIT};
        VkQueueFlags unique_queue_flags{};
        uint32_t num_graphics_queues{1};

        std::vector<VkQueueFamilyProperties> queue_families;
        std::unordered_set<uint32_t> selected_queue_families;
        std::vector<std::vector<float>> queue_priorities;

        void rebuild_pointers() {
            rebuild_pointer_list(instance_extension_pointers, instance_extensions);
            rebuild_pointer_list(instance_validation_layer_pointers, instance_validation_layers);
            rebuild_pointer_list(device_extension_pointers, device_extensions);
            rebuild_pointer_list(device_validation_layer_pointers, device_validation_layers);
        }

        context build(void* extension_chain) {
#ifdef VKZ_ENABLE_NSIGHT_AFTERMATH
            if (!aftermath::enable()) {
                warn("NVIDIA Nsight Aftermath is available but could not be enabled");
            }
#endif
            application_info.pApplicationName = app_name.c_str();
            application_info.pEngineName = engine_name.c_str();

            auto result = context{};
            result.api_version = application_info.apiVersion;
            result.instance = create_instance();
            volkLoadInstance(result.instance);
            result.surface = create_surface(result.instance);
            surface = result.surface;
            result.debug_messenger = create_debug_messenger(result.instance);
            result.device.physical = pick_physical_device(result.instance);
#ifdef VKZ_ENABLE_NSIGHT_AFTERMATH
            VkDeviceDiagnosticsConfigCreateInfoNV diagnostics{
                VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV};
            const std::vector<const char*> diagnostics_extensions{
                VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME};
            if (supports_device_extensions(result.device.physical, diagnostics_extensions)) {
                device_extensions.emplace_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);
                rebuild_pointers();
                diagnostics.flags =
                    VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
                    VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |
                    VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV;
                diagnostics.pNext = extension_chain;
                extension_chain = &diagnostics;
            } else {
                warn("VK_NV_device_diagnostics_config is unavailable; Aftermath dumps will contain reduced diagnostics");
            }
#endif
            result.device.logical = create_device(result.device.physical, extension_chain);
            volkLoadDevice(result.device.logical);

            return result;
        }

    private:
        static void rebuild_pointer_list(std::vector<const char*>& pointers, const std::vector<std::string>& strings) {
            pointers.clear();
            pointers.reserve(strings.size());

            for (const auto& string : strings) {
                pointers.push_back(string.c_str());
            }
        }

        VkSurfaceKHR create_surface(VkInstance instance) const {
            if (!surface_provider) {
                return {};
            }

            return (*surface_provider)(instance);
        }

        VkInstance create_instance() {
#ifndef NDEBUG
            instance_extension_pointers.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

            VKZ_CHECK_VULKAN(volkInitialize());

            if (!supports_instance_layers(instance_validation_layer_pointers)) {
                VKZ_THROW("One or more requested Vulkan instance layers are not available")
            }

            if (!supports_instance_extensions(instance_extension_pointers)) {
                VKZ_THROW("One or more requested Vulkan instance extensions are not available")
            }

            log_extensions("instance", instance_extension_pointers);

            VkInstanceCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create_info.pApplicationInfo = &application_info;
            create_info.enabledExtensionCount = VKZ_COUNT(instance_extension_pointers);
            create_info.ppEnabledExtensionNames = instance_extension_pointers.data();
            create_info.enabledLayerCount = VKZ_COUNT(instance_validation_layer_pointers);
            create_info.ppEnabledLayerNames = instance_validation_layer_pointers.data();

#ifndef NDEBUG
            VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
            if (contains(instance_extension_pointers, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                debug_create_info = debug_messenger_create_info();
                create_info.pNext = &debug_create_info;
            }
#endif

            VkInstance instance{};
            VKZ_CHECK_VULKAN(vkCreateInstance(&create_info, nullptr, &instance));
            return instance;
        }

        VkDebugUtilsMessengerEXT create_debug_messenger(VkInstance instance) const {
#ifdef NDEBUG
            return {};
#else
            if (!contains(instance_extension_pointers, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                return {};
            }

            if (!vkCreateDebugUtilsMessengerEXT) {
                return {};
            }

            const auto create_info = debug_messenger_create_info();

            VkDebugUtilsMessengerEXT messenger{};
            VKZ_CHECK_VULKAN(vkCreateDebugUtilsMessengerEXT(instance, &create_info, nullptr, &messenger));
            return messenger;
#endif
        }

        VkPhysicalDevice pick_physical_device(VkInstance instance) {
            uint32_t device_count{};
            VKZ_CHECK_VULKAN(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));

            if (device_count == 0) {
                VKZ_THROW("No Vulkan physical devices are available")
            }

            std::vector<VkPhysicalDevice> devices(device_count);
            VKZ_CHECK_VULKAN(vkEnumeratePhysicalDevices(instance, &device_count, devices.data()));

            for (auto physical_device : devices) {
                if (is_suitable(physical_device)) {
                    return physical_device;
                }
            }

            VKZ_THROW("No suitable Vulkan physical device was found")
        }

        bool is_suitable(VkPhysicalDevice physical_device) {
            if (!supports_device_extensions(physical_device, device_extension_pointers)) {
                return false;
            }

            if (!supports_device_layers(physical_device, device_validation_layer_pointers)) {
                return false;
            }

            uint32_t queue_family_count{};
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

            queue_families.resize(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

            return select_queue_families(physical_device);
        }

        bool select_queue_families(VkPhysicalDevice physical_device) {
            selected_queue_families.clear();

            for (auto bit : queue_bits(unique_queue_flags)) {
                const auto family = find_queue_family(physical_device, bit, surface, true);
                if (family == invalid_queue_family()) {
                    return false;
                }

                selected_queue_families.insert(family);
            }

            for (auto bit : queue_bits(queue_flags)) {
                const auto family = find_queue_family(physical_device, bit, surface, false);
                if (family == invalid_queue_family()) {
                    return false;
                }

                selected_queue_families.insert(family);
            }

            return true;
        }

        uint32_t find_queue_family(
                VkPhysicalDevice physical_device,
                VkQueueFlagBits flag,
                VkSurfaceKHR surface,
                bool unique) const {
            for (uint32_t i = 0; i < queue_families.size(); ++i) {
                const auto& family = queue_families[i];
                if (unique && selected_queue_families.contains(i)) {
                    continue;
                }

                if (!(family.queueFlags & flag)) {
                    continue;
                }

                if (surface) {
                    VkBool32 supports_present{};
                    VKZ_CHECK_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supports_present));
                    if (!supports_present) {
                        continue;
                    }
                }

                return i;
            }

            return invalid_queue_family();
        }

        static constexpr uint32_t invalid_queue_family() {
            return std::numeric_limits<uint32_t>::max();
        }

        VkDevice create_device(VkPhysicalDevice physical_device, void* extension_chain) {
            queue_priorities.clear();

            std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
            queue_create_infos.reserve(selected_queue_families.size());

            for (auto family_index : selected_queue_families) {
                const auto queue_count = queue_count_for_family(family_index);

                auto& priorities = queue_priorities.emplace_back(queue_count, 1.0f);

                VkDeviceQueueCreateInfo create_info{};
                create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                create_info.queueFamilyIndex = family_index;
                create_info.queueCount = queue_count;
                create_info.pQueuePriorities = priorities.data();

                queue_create_infos.push_back(create_info);
            }

            VkDeviceCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            create_info.pNext = extension_chain;
            create_info.queueCreateInfoCount = VKZ_COUNT(queue_create_infos);
            create_info.pQueueCreateInfos = queue_create_infos.data();
            create_info.enabledExtensionCount = VKZ_COUNT(device_extension_pointers);
            create_info.ppEnabledExtensionNames = device_extension_pointers.data();
            create_info.enabledLayerCount = VKZ_COUNT(device_validation_layer_pointers);
            create_info.ppEnabledLayerNames = device_validation_layer_pointers.data();
            create_info.pEnabledFeatures = &enabled_features;

            log_extensions("device", device_extension_pointers);

            VkDevice device{};
            VKZ_CHECK_VULKAN(vkCreateDevice(physical_device, &create_info, nullptr, &device));
            return device;
        }

        uint32_t queue_count_for_family(uint32_t family_index) const {
            auto count = 1u;

            if (queue_families[family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                count = std::max(count, num_graphics_queues);
            }

            return std::min(count, queue_families[family_index].queueCount);
        }
    };

    vkz::builder context::builder() {
        return {};
    }

    context context::create_not_owned(
            VkInstance instance,
            vkz::device device,
            VkSurfaceKHR surface,
            VkDebugUtilsMessengerEXT debug_messenger,
            uint32_t api_version) {
        if (instance && volkGetLoadedInstance() != instance) {
            VKZ_THROW("Cannot create a non-owning context: the supplied Vulkan instance is not loaded by Volk")
        }
        if (device.logical && volkGetLoadedDevice() != device.logical) {
            VKZ_THROW("Cannot create a non-owning context: the supplied Vulkan device is not loaded by Volk")
        }

        context result;
        result.instance = instance;
        result.debug_messenger = debug_messenger;
        result.surface = surface;
        result.device = device;
        result.api_version = api_version;
        result.owns_components_ = false;

        return result;
    }

    context::~context() {
        if (owns_components_) {
            destroy_context(*this);
        }
    }

    context::context(context&& other) noexcept
        : instance{std::exchange(other.instance, nullptr)}
        , debug_messenger{std::exchange(other.debug_messenger, nullptr)}
        , surface{std::exchange(other.surface, nullptr)}
        , device{std::exchange(other.device, {})}
        , api_version{std::exchange(other.api_version, VK_API_VERSION_1_3)}
        , owns_components_{std::exchange(other.owns_components_, false)} {
    }

    context& context::operator=(context&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (owns_components_) {
            destroy_context(*this);
        }

        instance = std::exchange(other.instance, nullptr);
        debug_messenger = std::exchange(other.debug_messenger, nullptr);
        surface = std::exchange(other.surface, nullptr);
        device = std::exchange(other.device, {});
        api_version = std::exchange(other.api_version, VK_API_VERSION_1_3);
        owns_components_ = std::exchange(other.owns_components_, false);

        return *this;
    }

    builder::builder()
        : pimpl{new Impl{}} {
    }

    builder::~builder() {
        delete pimpl;
    }

    builder::builder(builder&& other) noexcept
        : _extensions{std::move(other._extensions)}
        , pimpl{std::exchange(other.pimpl, nullptr)} {
    }

    builder& builder::operator=(builder&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        delete pimpl;
        _extensions = std::move(other._extensions);
        pimpl = std::exchange(other.pimpl, nullptr);

        return *this;
    }

    builder& builder::app_name(const std::string& app_name) {
        pimpl->app_name = app_name;
        pimpl->application_info.pApplicationName = pimpl->app_name.c_str();
        return *this;
    }

    builder& builder::app_version(const std::string& app_version) {
        pimpl->application_info.applicationVersion = parse_version(app_version);
        return *this;
    }

    builder& builder::engine_name(const std::string& engine_name) {
        pimpl->engine_name = engine_name;
        pimpl->application_info.pEngineName = pimpl->engine_name.c_str();
        return *this;
    }

    builder& builder::engine_version(const std::string& engine_version) {
        pimpl->application_info.engineVersion = parse_version(engine_version);
        return *this;
    }

    builder& builder::api_version(uint api_version) {
        pimpl->application_info.apiVersion = api_version;
        return *this;
    }

    builder& builder::add_instance_extension(const std::string& extension) {
        pimpl->instance_extensions.push_back(extension);
        pimpl->rebuild_pointers();
        return *this;
    }

    builder& builder::add_instance_layer(const std::string& layer) {
        pimpl->instance_validation_layers.push_back(layer);
        pimpl->rebuild_pointers();
        return *this;
    }

    builder& builder::add_device_extension(const std::string& extension) {
        pimpl->device_extensions.push_back(extension);
        pimpl->rebuild_pointers();
        return *this;
    }

    builder& builder::add_device_layer(const std::string& layer) {
        pimpl->device_validation_layers.push_back(layer);
        pimpl->rebuild_pointers();
        return *this;
    }

    builder& builder::enabled_features(const VkPhysicalDeviceFeatures& features) {
        pimpl->enabled_features = features;
        return *this;
    }

    builder& builder::surface(const surface_provider& surface) {
        pimpl->surface_provider = &surface;
        return *this;
    }

    builder& builder::add_queue(VkQueueFlagBits flag) {
        pimpl->queue_flags |= flag;
        return *this;
    }

    builder& builder::add_unique_queue(VkQueueFlagBits flag) {
        pimpl->unique_queue_flags |= flag;
        return *this;
    }

    builder& builder::num_graphics_queues(uint count) {
        pimpl->num_graphics_queues = std::max(1u, count);
        return *this;
    }

    builder& builder::add_extension_chain(const device_extension_chain& extensions) {
        _extensions.add(extensions);
        return *this;
    }

    context builder::build() {
        pimpl->rebuild_pointers();
        return pimpl->build(_extensions.head());
    }
}
