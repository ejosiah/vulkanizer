#include "vulkanizer/context.hpp"

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
        VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
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

        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo() {
            VkDebugUtilsMessengerCreateInfoEXT createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType =
                    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = debugCallback;
            return createInfo;
        }

        uint32_t parseVersion(const std::string& value) {
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

        std::vector<VkQueueFlagBits> queueBits(VkQueueFlags flags) {
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

        bool supportsInstanceLayers(const std::vector<const char*>& layers) {
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

        bool supportsInstanceExtensions(const std::vector<const char*>& extensions) {
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

        bool supportsDeviceExtensions(VkPhysicalDevice physicalDevice, const std::vector<const char*>& extensions) {
            uint32_t count{};
            VKZ_CHECK_VULKAN(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr));

            std::vector<VkExtensionProperties> available(count);
            VKZ_CHECK_VULKAN(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, available.data()));

            return std::ranges::all_of(extensions, [&available](const char* extension) {
                return std::ranges::any_of(available, [extension](const auto& properties) {
                    return std::string_view{properties.extensionName} == extension;
                });
            });
        }

        bool supportsDeviceLayers(VkPhysicalDevice physicalDevice, const std::vector<const char*>& layers) {
            uint32_t count{};
            VKZ_CHECK_VULKAN(vkEnumerateDeviceLayerProperties(physicalDevice, &count, nullptr));

            std::vector<VkLayerProperties> available(count);
            VKZ_CHECK_VULKAN(vkEnumerateDeviceLayerProperties(physicalDevice, &count, available.data()));

            return std::ranges::all_of(layers, [&available](const char* layer) {
                return std::ranges::any_of(available, [layer](const auto& properties) {
                    return std::string_view{properties.layerName} == layer;
                });
            });
        }

        void destroyContext(context& context) {
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
            if (context.debugMessenger && vkDestroyDebugUtilsMessengerEXT) {
                vkDestroyDebugUtilsMessengerEXT(context.instance, context.debugMessenger, nullptr);
                context.debugMessenger = nullptr;
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
        std::string appName{"vulkanizer"};
        std::string engineName{"vulkanizer"};
        VkApplicationInfo applicationInfo{
                VK_STRUCTURE_TYPE_APPLICATION_INFO,
                nullptr,
                appName.c_str(),
                0,
                engineName.c_str(),
                0,
                VK_API_VERSION_1_3,
        };

        std::vector<std::string> instanceExtensions;
        std::vector<std::string> instanceValidationLayers;
        std::vector<std::string> deviceExtensions;
        std::vector<std::string> deviceValidationLayers;

        std::vector<const char*> instanceExtensionPointers;
        std::vector<const char*> instanceValidationLayerPointers;
        std::vector<const char*> deviceExtensionPointers;
        std::vector<const char*> deviceValidationLayerPointers;

        VkPhysicalDeviceFeatures enabledFeatures{};
        const surface_provider* surfaceProvider{};
        VkSurfaceKHR surface{};

        VkQueueFlags queueFlags{VK_QUEUE_GRAPHICS_BIT};
        VkQueueFlags uniqueQueueFlags{};
        uint32_t numGraphicsQueues{1};

        std::vector<VkQueueFamilyProperties> queueFamilies;
        std::unordered_set<uint32_t> selectedQueueFamilies;
        std::vector<std::vector<float>> queuePriorities;

        void rebuildPointers() {
            rebuildPointerList(instanceExtensionPointers, instanceExtensions);
            rebuildPointerList(instanceValidationLayerPointers, instanceValidationLayers);
            rebuildPointerList(deviceExtensionPointers, deviceExtensions);
            rebuildPointerList(deviceValidationLayerPointers, deviceValidationLayers);
        }

        static void destroyExtensionChain(void* extensions) {
            auto* node = static_cast<VkBaseOutStructure*>(extensions);

            while (node) {
                auto* next = static_cast<VkBaseOutStructure*>(node->pNext);
                ::operator delete(node);
                node = next;
            }
        }

        context build(void* extensionChain) {
            applicationInfo.pApplicationName = appName.c_str();
            applicationInfo.pEngineName = engineName.c_str();

            auto result = context{};
            result.instance = createInstance();
            volkLoadInstance(result.instance);
            result.surface = createSurface(result.instance);
            surface = result.surface;
            result.debugMessenger = createDebugMessenger(result.instance);
            result.device.physical = pickPhysicalDevice(result.instance);
            result.device.logical = createDevice(result.device.physical, extensionChain);
            volkLoadDevice(result.device.logical);

            return result;
        }

    private:
        static void rebuildPointerList(std::vector<const char*>& pointers, const std::vector<std::string>& strings) {
            pointers.clear();
            pointers.reserve(strings.size());

            for (const auto& string : strings) {
                pointers.push_back(string.c_str());
            }
        }

        VkSurfaceKHR createSurface(VkInstance instance) const {
            if (!surfaceProvider) {
                return {};
            }

            return (*surfaceProvider)(instance);
        }

        VkInstance createInstance() const {
            VKZ_CHECK_VULKAN(volkInitialize());

            if (!supportsInstanceLayers(instanceValidationLayerPointers)) {
                VKZ_THROW("One or more requested Vulkan instance layers are not available")
            }

            if (!supportsInstanceExtensions(instanceExtensionPointers)) {
                VKZ_THROW("One or more requested Vulkan instance extensions are not available")
            }

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &applicationInfo;
            createInfo.enabledExtensionCount = VKZ_COUNT(instanceExtensionPointers);
            createInfo.ppEnabledExtensionNames = instanceExtensionPointers.data();
            createInfo.enabledLayerCount = VKZ_COUNT(instanceValidationLayerPointers);
            createInfo.ppEnabledLayerNames = instanceValidationLayerPointers.data();

#ifndef NDEBUG
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            if (contains(instanceExtensionPointers, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                debugCreateInfo = debugMessengerCreateInfo();
                createInfo.pNext = &debugCreateInfo;
            }
#endif

            VkInstance instance{};
            VKZ_CHECK_VULKAN(vkCreateInstance(&createInfo, nullptr, &instance));
            return instance;
        }

        VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance) const {
#ifdef NDEBUG
            return {};
#else
            if (!contains(instanceExtensionPointers, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                return {};
            }

            if (!vkCreateDebugUtilsMessengerEXT) {
                return {};
            }

            const auto createInfo = debugMessengerCreateInfo();

            VkDebugUtilsMessengerEXT messenger{};
            VKZ_CHECK_VULKAN(vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &messenger));
            return messenger;
#endif
        }

        VkPhysicalDevice pickPhysicalDevice(VkInstance instance) {
            uint32_t deviceCount{};
            VKZ_CHECK_VULKAN(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

            if (deviceCount == 0) {
                VKZ_THROW("No Vulkan physical devices are available")
            }

            std::vector<VkPhysicalDevice> devices(deviceCount);
            VKZ_CHECK_VULKAN(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

            for (auto physicalDevice : devices) {
                if (isSuitable(physicalDevice)) {
                    return physicalDevice;
                }
            }

            VKZ_THROW("No suitable Vulkan physical device was found")
        }

        bool isSuitable(VkPhysicalDevice physicalDevice) {
            if (!supportsDeviceExtensions(physicalDevice, deviceExtensionPointers)) {
                return false;
            }

            if (!supportsDeviceLayers(physicalDevice, deviceValidationLayerPointers)) {
                return false;
            }

            uint32_t queueFamilyCount{};
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

            queueFamilies.resize(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

            return selectQueueFamilies(physicalDevice);
        }

        bool selectQueueFamilies(VkPhysicalDevice physicalDevice) {
            selectedQueueFamilies.clear();

            for (auto bit : queueBits(uniqueQueueFlags)) {
                const auto family = findQueueFamily(physicalDevice, bit, surface, true);
                if (family == invalidQueueFamily()) {
                    return false;
                }

                selectedQueueFamilies.insert(family);
            }

            for (auto bit : queueBits(queueFlags)) {
                const auto family = findQueueFamily(physicalDevice, bit, surface, false);
                if (family == invalidQueueFamily()) {
                    return false;
                }

                selectedQueueFamilies.insert(family);
            }

            return true;
        }

        uint32_t findQueueFamily(
                VkPhysicalDevice physicalDevice,
                VkQueueFlagBits flag,
                VkSurfaceKHR surface,
                bool unique) const {
            for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
                const auto& family = queueFamilies[i];
                if (unique && selectedQueueFamilies.contains(i)) {
                    continue;
                }

                if (!(family.queueFlags & flag)) {
                    continue;
                }

                if (surface) {
                    VkBool32 supportsPresent{};
                    VKZ_CHECK_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresent));
                    if (!supportsPresent) {
                        continue;
                    }
                }

                return i;
            }

            return invalidQueueFamily();
        }

        static constexpr uint32_t invalidQueueFamily() {
            return std::numeric_limits<uint32_t>::max();
        }

        VkDevice createDevice(VkPhysicalDevice physicalDevice, void* extensionChain) {
            queuePriorities.clear();

            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            queueCreateInfos.reserve(selectedQueueFamilies.size());

            for (auto familyIndex : selectedQueueFamilies) {
                const auto queueCount = queueCountForFamily(familyIndex);

                auto& priorities = queuePriorities.emplace_back(queueCount, 1.0f);

                VkDeviceQueueCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                createInfo.queueFamilyIndex = familyIndex;
                createInfo.queueCount = queueCount;
                createInfo.pQueuePriorities = priorities.data();

                queueCreateInfos.push_back(createInfo);
            }

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pNext = extensionChain;
            createInfo.queueCreateInfoCount = VKZ_COUNT(queueCreateInfos);
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.enabledExtensionCount = VKZ_COUNT(deviceExtensionPointers);
            createInfo.ppEnabledExtensionNames = deviceExtensionPointers.data();
            createInfo.enabledLayerCount = VKZ_COUNT(deviceValidationLayerPointers);
            createInfo.ppEnabledLayerNames = deviceValidationLayerPointers.data();
            createInfo.pEnabledFeatures = &enabledFeatures;

            VkDevice device{};
            VKZ_CHECK_VULKAN(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));
            return device;
        }

        uint32_t queueCountForFamily(uint32_t familyIndex) const {
            auto count = 1u;

            if (queueFamilies[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                count = std::max(count, numGraphicsQueues);
            }

            return std::min(count, queueFamilies[familyIndex].queueCount);
        }
    };

    vkz::builder context::builder() {
        return {};
    }

    context::~context() {
        destroyContext(*this);
    }

    context::context(context&& other) noexcept
        : instance{std::exchange(other.instance, nullptr)}
        , debugMessenger{std::exchange(other.debugMessenger, nullptr)}
        , surface{std::exchange(other.surface, nullptr)}
        , device{std::exchange(other.device, {})} {
    }

    context& context::operator=(context&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        destroyContext(*this);

        instance = std::exchange(other.instance, nullptr);
        debugMessenger = std::exchange(other.debugMessenger, nullptr);
        surface = std::exchange(other.surface, nullptr);
        device = std::exchange(other.device, {});

        return *this;
    }

    builder::builder()
        : pimpl{new Impl{}} {
    }

    builder::~builder() {
        Impl::destroyExtensionChain(_extensions);
        delete pimpl;
    }

    builder::builder(builder&& other) noexcept
        : _extensions{std::exchange(other._extensions, nullptr)}
        , pimpl{std::exchange(other.pimpl, nullptr)} {
    }

    builder& builder::operator=(builder&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        Impl::destroyExtensionChain(_extensions);
        delete pimpl;
        _extensions = std::exchange(other._extensions, nullptr);
        pimpl = std::exchange(other.pimpl, nullptr);

        return *this;
    }

    builder& builder::appName(const std::string& appName) {
        pimpl->appName = appName;
        pimpl->applicationInfo.pApplicationName = pimpl->appName.c_str();
        return *this;
    }

    builder& builder::appVersion(const std::string& appVersion) {
        pimpl->applicationInfo.applicationVersion = parseVersion(appVersion);
        return *this;
    }

    builder& builder::engineName(const std::string& engineName) {
        pimpl->engineName = engineName;
        pimpl->applicationInfo.pEngineName = pimpl->engineName.c_str();
        return *this;
    }

    builder& builder::engineVersion(const std::string& engineVersion) {
        pimpl->applicationInfo.engineVersion = parseVersion(engineVersion);
        return *this;
    }

    builder& builder::apiVersion(uint apiVersion) {
        pimpl->applicationInfo.apiVersion = apiVersion;
        return *this;
    }

    builder& builder::addInstanceExtension(const std::string& extension) {
        pimpl->instanceExtensions.push_back(extension);
        pimpl->rebuildPointers();
        return *this;
    }

    builder& builder::addInstanceLayer(const std::string& layer) {
        pimpl->instanceValidationLayers.push_back(layer);
        pimpl->rebuildPointers();
        return *this;
    }

    builder& builder::addDeviceExtension(const std::string& extension) {
        pimpl->deviceExtensions.push_back(extension);
        pimpl->rebuildPointers();
        return *this;
    }

    builder& builder::addDeviceLayer(const std::string& layer) {
        pimpl->deviceValidationLayers.push_back(layer);
        pimpl->rebuildPointers();
        return *this;
    }

    builder& builder::enabledFeatures(const VkPhysicalDeviceFeatures& features) {
        pimpl->enabledFeatures = features;
        return *this;
    }

    builder& builder::surface(const surface_provider& surface) {
        pimpl->surfaceProvider = &surface;
        return *this;
    }

    builder& builder::addQueue(VkQueueFlagBits flag) {
        pimpl->queueFlags |= flag;
        return *this;
    }

    builder& builder::addUniqueQueue(VkQueueFlagBits flag) {
        pimpl->uniqueQueueFlags |= flag;
        return *this;
    }

    builder& builder::numGraphicsQueues(uint count) {
        pimpl->numGraphicsQueues = std::max(1u, count);
        return *this;
    }

    context builder::build() {
        pimpl->rebuildPointers();
        return pimpl->build(_extensions);
    }
}
