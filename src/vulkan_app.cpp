#include <vulkanizer/vulkan_app.hpp>

#include <vulkanizer/log.hpp>
#include <vulkanizer/status.hpp>

#include <cmath>

namespace {
    vkz::context create_context(
            const vkz::vulkan_app_create_info& create_info,
            const vkz::surface_provider& surface_provider) {
        uint32_t required_extension_count{};
        const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_extension_count);
        if (!required_extensions) {
            VKZ_THROW("GLFW could not provide Vulkan instance extensions")
        }

        VkPhysicalDeviceSynchronization2Features synchronization2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            nullptr,
            create_info.synchronization2 ? VK_TRUE : VK_FALSE,
        };
        VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            nullptr,
            create_info.dynamic_rendering ? VK_TRUE : VK_FALSE,
        };
        VkPhysicalDeviceVulkan11Features vulkan11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        vulkan11.multiview = create_info.multiview ? VK_TRUE : VK_FALSE;

        const auto& features = create_info.enabled_features;
        const VkPhysicalDeviceFeatures enabled_features{
            .robustBufferAccess = features.robust_buffer_access,
            .fullDrawIndexUint32 = features.full_draw_index_uint32,
            .imageCubeArray = features.image_cube_array,
            .independentBlend = features.independent_blend,
            .geometryShader = features.geometry_shader,
            .tessellationShader = features.tessellation_shader,
            .sampleRateShading = features.sample_rate_shading,
            .dualSrcBlend = features.dual_src_blend,
            .logicOp = features.logic_op,
            .multiDrawIndirect = features.multi_draw_indirect,
            .drawIndirectFirstInstance = features.draw_indirect_first_instance,
            .depthClamp = features.depth_clamp,
            .depthBiasClamp = features.depth_bias_clamp,
            .fillModeNonSolid = features.fill_mode_non_solid,
            .depthBounds = features.depth_bounds,
            .wideLines = features.wide_lines,
            .largePoints = features.large_points,
            .alphaToOne = features.alpha_to_one,
            .multiViewport = features.multi_viewport,
            .samplerAnisotropy = features.sampler_anisotropy,
            .textureCompressionETC2 = features.texture_compression_etc2,
            .textureCompressionASTC_LDR = features.texture_compression_astc_ldr,
            .textureCompressionBC = features.texture_compression_bc,
            .occlusionQueryPrecise = features.occlusion_query_precise,
            .pipelineStatisticsQuery = features.pipeline_statistics_query,
            .vertexPipelineStoresAndAtomics = features.vertex_pipeline_stores_and_atomics,
            .fragmentStoresAndAtomics = features.fragment_stores_and_atomics,
            .shaderTessellationAndGeometryPointSize = features.shader_tessellation_and_geometry_point_size,
            .shaderImageGatherExtended = features.shader_image_gather_extended,
            .shaderStorageImageExtendedFormats = features.shader_storage_image_extended_formats,
            .shaderStorageImageMultisample = features.shader_storage_image_multisample,
            .shaderStorageImageReadWithoutFormat = features.shader_storage_image_read_without_format,
            .shaderStorageImageWriteWithoutFormat = features.shader_storage_image_write_without_format,
            .shaderUniformBufferArrayDynamicIndexing = features.shader_uniform_buffer_array_dynamic_indexing,
            .shaderSampledImageArrayDynamicIndexing = features.shader_sampled_image_array_dynamic_indexing,
            .shaderStorageBufferArrayDynamicIndexing = features.shader_storage_buffer_array_dynamic_indexing,
            .shaderStorageImageArrayDynamicIndexing = features.shader_storage_image_array_dynamic_indexing,
            .shaderClipDistance = features.shader_clip_distance,
            .shaderCullDistance = features.shader_cull_distance,
            .shaderFloat64 = features.shader_float64,
            .shaderInt64 = features.shader_int64,
            .shaderInt16 = features.shader_int16,
            .shaderResourceResidency = features.shader_resource_residency,
            .shaderResourceMinLod = features.shader_resource_min_lod,
            .sparseBinding = features.sparse_binding,
            .sparseResidencyBuffer = features.sparse_residency_buffer,
            .sparseResidencyImage2D = features.sparse_residency_image_2d,
            .sparseResidencyImage3D = features.sparse_residency_image_3d,
            .sparseResidency2Samples = features.sparse_residency_2_samples,
            .sparseResidency4Samples = features.sparse_residency_4_samples,
            .sparseResidency8Samples = features.sparse_residency_8_samples,
            .sparseResidency16Samples = features.sparse_residency_16_samples,
            .sparseResidencyAliased = features.sparse_residency_aliased,
            .variableMultisampleRate = features.variable_multisample_rate,
            .inheritedQueries = features.inherited_queries,
        };

        auto builder = vkz::context::builder();
        builder
            .app_name(create_info.title)
            .api_version(VK_API_VERSION_1_3)
            .surface(surface_provider)
            .enabled_features(enabled_features)
            .add_device_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        if (create_info.engine_name) {
            builder.engine_name(create_info.engine_name);
        }
        if (create_info.validation) {
            builder.add_instance_layer("VK_LAYER_KHRONOS_validation");
        }

        if (create_info.synchronization2) {
            builder.add_extension(synchronization2);
        }
        if (create_info.dynamic_rendering) {
            builder.add_extension(dynamic_rendering);
        }
        if (create_info.multiview) {
            builder.add_extension(vulkan11);
        }

        for (uint32_t i = 0; i < required_extension_count; ++i) {
            builder.add_instance_extension(required_extensions[i]);
        }
        for (const auto* extension : create_info.instance_extensions) {
            builder.add_instance_extension(extension);
        }
        for (const auto* extension : create_info.device_extensions) {
            builder.add_device_extension(extension);
        }

        builder.add_extension_chain(create_info.extension_chain);

        return builder.build();
    }
}

namespace vkz {

    glfw_surface_provider::glfw_surface_provider(GLFWwindow* window)
        : window_{window} {
    }

    VkSurfaceKHR glfw_surface_provider::operator()(VkInstance instance) const {
        VkSurfaceKHR surface{};
        VKZ_CHECK_VULKAN(glfwCreateWindowSurface(instance, window_, nullptr, &surface));
        return surface;
    }

    glfw_runtime::glfw_runtime() {
        if (!glfwInit()) {
            VKZ_THROW("Failed to initialize GLFW")
        }
    }

    glfw_runtime::~glfw_runtime() {
        glfwTerminate();
    }

    void glfw_window_deleter::operator()(GLFWwindow* window) const {
        if (window) {
            glfwDestroyWindow(window);
        }
    }

    vulkan_app::vulkan_app(const vulkan_app_create_info& create_info)
        : surface_provider_{nullptr}, vsync_{create_info.vsync} {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, create_info.resizable ? GLFW_TRUE : GLFW_FALSE);
        window_.reset(glfwCreateWindow(
            static_cast<int>(create_info.width),
            static_cast<int>(create_info.height),
            create_info.title,
            nullptr,
            nullptr));
        if (!window_) {
            VKZ_THROW("Failed to create GLFW window")
        }

        surface_provider_ = glfw_surface_provider{window_.get()};
        context_ = create_context(create_info, surface_provider_);
        queue_family_index_ = find_graphics_present_queue_family(context_.device.physical, context_.surface);
        vkGetDeviceQueue(context_.device.logical, queue_family_index_, 0, &graphics_queue_);
    }

    vulkan_app::~vulkan_app() {
        context_ = {};
    }

    GLFWwindow* vulkan_app::window() const {
        return window_.get();
    }

    vkz::context& vulkan_app::context() {
        return context_;
    }

    const vkz::context& vulkan_app::context() const {
        return context_;
    }

    uint32_t vulkan_app::queue_family_index() const {
        return queue_family_index_;
    }

    VkQueue vulkan_app::graphics_queue() const {
        return graphics_queue_;
    }

    bool vulkan_app::should_close() const {
        return glfwWindowShouldClose(window_.get());
    }

    void vulkan_app::poll_events() const {
        glfwPollEvents();
    }

    void vulkan_app::wait_for_drawable_window() const {
        int width{};
        int height{};
        glfwGetFramebufferSize(window_.get(), &width, &height);

        while ((width == 0 || height == 0) && !glfwWindowShouldClose(window_.get())) {
            glfwWaitEvents();
            glfwGetFramebufferSize(window_.get(), &width, &height);
        }
    }

    std::unique_ptr<vkz::swapchain> vulkan_app::create_swapchain(VkImageUsageFlags image_usage) const {
        auto builder = vkz::swapchain::builder(context_);
        builder.set_image_usage(image_usage);
        if (!vsync_) builder.set_present_mode(VK_PRESENT_MODE_MAILBOX_KHR);
        return std::make_unique<vkz::swapchain>(builder.build());
    }

    uint32_t find_graphics_present_queue_family(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
        uint32_t queue_family_count{};
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

        for (uint32_t i = 0; i < queue_families.size(); ++i) {
            VkBool32 present_supported{};
            VKZ_CHECK_VULKAN(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_supported));
            if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_supported) {
                return i;
            }
        }

        VKZ_THROW("No graphics/present queue family is available")
    }

    VkCommandPool create_command_pool(VkDevice device, uint32_t queue_family_index, VkCommandPoolCreateFlags flags) {
        VkCommandPoolCreateInfo create_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        create_info.flags = flags;
        create_info.queueFamilyIndex = queue_family_index;

        VkCommandPool command_pool{};
        VKZ_CHECK_VULKAN(vkCreateCommandPool(device, &create_info, nullptr, &command_pool));
        return command_pool;
    }

    VkCommandBuffer begin_command_buffer(VkDevice device, VkCommandPool command_pool) {
        VkCommandBufferAllocateInfo allocate_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.commandPool = command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer{};
        VKZ_CHECK_VULKAN(vkAllocateCommandBuffers(device, &allocate_info, &command_buffer));

        VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKZ_CHECK_VULKAN(vkBeginCommandBuffer(command_buffer, &begin_info));
        return command_buffer;
    }

    void submit_and_free(
            VkDevice device,
            VkQueue queue,
            VkCommandPool command_pool,
            VkCommandBuffer command_buffer,
            VkSemaphore wait_semaphore,
            VkSemaphore signal_semaphore,
            VkFence fence) {
        VKZ_CHECK_VULKAN(vkEndCommandBuffer(command_buffer));

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.waitSemaphoreCount = wait_semaphore ? 1u : 0u;
        submit_info.pWaitSemaphores = wait_semaphore ? &wait_semaphore : nullptr;
        submit_info.pWaitDstStageMask = wait_semaphore ? &wait_stage : nullptr;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;
        submit_info.signalSemaphoreCount = signal_semaphore ? 1u : 0u;
        submit_info.pSignalSemaphores = signal_semaphore ? &signal_semaphore : nullptr;

        VKZ_CHECK_VULKAN(vkQueueSubmit(queue, 1, &submit_info, fence));
        if (fence) {
            VKZ_CHECK_VULKAN(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
            VKZ_CHECK_VULKAN(vkResetFences(device, 1, &fence));
        } else {
            VKZ_CHECK_VULKAN(vkQueueWaitIdle(queue));
        }

        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    }

    VkSemaphore create_semaphore(VkDevice device) {
        VkSemaphoreCreateInfo create_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore semaphore{};
        VKZ_CHECK_VULKAN(vkCreateSemaphore(device, &create_info, nullptr, &semaphore));
        return semaphore;
    }

    VkFence create_fence(VkDevice device) {
        VkFenceCreateInfo create_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fence{};
        VKZ_CHECK_VULKAN(vkCreateFence(device, &create_info, nullptr, &fence));
        return fence;
    }

    VkFormat pick_depth_format(VkPhysicalDevice physical_device) {
        constexpr VkFormat candidates[] = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        };

        for (const auto format : candidates) {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);

            if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                return format;
            }
        }

        VKZ_THROW("No supported depth attachment format was found")
    }

    std::vector<vkz::image_view> create_swapchain_image_views(vkz::device device, vkz::swapchain& swapchain) {
        std::vector<vkz::image_view> image_views;
        image_views.reserve(swapchain.image_count());

        for (uint32_t i = 0; i < swapchain.image_count(); ++i) {
            vkz::image swapchain_image{};
            swapchain_image.handle = swapchain.get_image(i);
            swapchain_image.create_info.format = swapchain.format();
            image_views.push_back(
                vkz::image_view::builder(device)
                    .image(swapchain_image)
                    .view_type(VK_IMAGE_VIEW_TYPE_2D)
                    .format(swapchain.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .level_count(1)
                    .layer_count(1)
                    .build());
        }

        return image_views;
    }

    void destroy_image_views(VkDevice device, std::vector<vkz::image_view>& image_views) {
        for (auto& image_view : image_views) {
            vkDestroyImageView(device, image_view.handle, nullptr);
        }
        image_views.clear();
    }
}
