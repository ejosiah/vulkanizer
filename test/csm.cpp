#define VKZ_IOSTREAM_ADAPTER

#include <vulkanizer/barrier.hpp>
#include <vulkanizer/context.hpp>
#include <vulkanizer/csm.hpp>
#include <vulkanizer/descriptor_set_builder.hpp>
#include <vulkanizer/graphics_pipeline_builder.hpp>
#include <vulkanizer/imgui.hpp>
#include <vulkanizer/io.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/memory.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/swapchain.hpp>
#include <vulkanizer/transforms.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {
    constexpr uint32_t window_width = 1440;
    constexpr uint32_t window_height = 900;
    constexpr uint32_t max_frames_in_flight = 2;

    struct vertex {
        glm::vec3 position{};
        glm::vec3 normal{};
    };

    struct object {
        VkBuffer vertices{};
        uint32_t vertex_count{};
        glm::mat4 transform{1};
    };

    struct scene_data {
        glm::mat4 view{1};
        glm::vec4 light_dir{0.4f, 0.7f, 0.2f, 0.0f};
        int num_cascades{4};
        int use_pcf{};
        int color_cascades{};
        int show_extents{};
        int color_shadow{};
        int camera_frozen{};
    };

    struct shadow_push_constants {
        glm::mat4 world{1};
        int cascade_index{};
    };

    struct render_push_constants {
        glm::mat4 world{1};
        glm::mat4 view_projection{1};
    };

    class glfw_surface_provider final : public vkz::surface_provider {
    public:
        explicit glfw_surface_provider(GLFWwindow* window)
            : window_{window} {}

        VkSurfaceKHR operator()(VkInstance instance) const override {
            VkSurfaceKHR surface{};
            VKZ_CHECK_VULKAN(glfwCreateWindowSurface(instance, window_, nullptr, &surface));
            return surface;
        }

    private:
        GLFWwindow* window_{};
    };

    std::string shader_path(const char* name) {
        return std::string{VKZ_CSM_TEST_SHADER_DIR} + "/" + name + ".spv";
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
            VkSemaphore wait_semaphore = {},
            VkSemaphore signal_semaphore = {},
            VkFence fence = {}) {
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

    std::vector<vkz::image_view> create_swapchain_image_views(VkDevice device, vkz::swapchain& swapchain) {
        std::vector<vkz::image_view> image_views;
        image_views.reserve(swapchain.image_count());

        for (uint32_t i = 0; i < swapchain.image_count(); ++i) {
            image_views.push_back(
                vkz::image_view::builder(device)
                    .image(swapchain.get_image(i))
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

    VkRenderPass create_render_pass(VkDevice device, VkFormat color_format, VkFormat depth_format) {
        std::array<VkAttachmentDescription, 2> attachments{};
        attachments[0].format = color_format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        attachments[1].format = depth_format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depth_attachment{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        subpass.pDepthStencilAttachment = &depth_attachment;

        VkRenderPassCreateInfo create_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;

        VkRenderPass render_pass{};
        VKZ_CHECK_VULKAN(vkCreateRenderPass(device, &create_info, nullptr, &render_pass));
        return render_pass;
    }

    std::vector<VkFramebuffer> create_framebuffers(
            VkDevice device,
            VkRenderPass render_pass,
            const std::vector<vkz::image_view>& color_views,
            VkImageView depth_view,
            uint32_t width,
            uint32_t height) {
        std::vector<VkFramebuffer> framebuffers;
        framebuffers.reserve(color_views.size());

        for (const auto& color_view : color_views) {
            std::array<VkImageView, 2> attachments{color_view.handle, depth_view};
            VkFramebufferCreateInfo create_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            create_info.renderPass = render_pass;
            create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            create_info.pAttachments = attachments.data();
            create_info.width = width;
            create_info.height = height;
            create_info.layers = 1;

            VkFramebuffer framebuffer{};
            VKZ_CHECK_VULKAN(vkCreateFramebuffer(device, &create_info, nullptr, &framebuffer));
            framebuffers.push_back(framebuffer);
        }

        return framebuffers;
    }

    void destroy_framebuffers(VkDevice device, std::vector<VkFramebuffer>& framebuffers) {
        for (auto framebuffer : framebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        framebuffers.clear();
    }

    void wait_for_drawable_window(GLFWwindow* window) {
        int width{};
        int height{};
        glfwGetFramebufferSize(window, &width, &height);
        while ((width == 0 || height == 0) && !glfwWindowShouldClose(window)) {
            glfwWaitEvents();
            glfwGetFramebufferSize(window, &width, &height);
        }
    }

    std::vector<vertex> make_plane(float size) {
        const float h = size * 0.5f;
        const glm::vec3 n{0.0f, 1.0f, 0.0f};
        return {
            {{-h, 0.0f, -h}, n}, {{ h, 0.0f,  h}, n}, {{ h, 0.0f, -h}, n},
            {{-h, 0.0f, -h}, n}, {{-h, 0.0f,  h}, n}, {{ h, 0.0f,  h}, n},
        };
    }

    void add_face(std::vector<vertex>& vertices, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 normal) {
        vertices.push_back({a, normal});
        vertices.push_back({b, normal});
        vertices.push_back({c, normal});
        vertices.push_back({a, normal});
        vertices.push_back({c, normal});
        vertices.push_back({d, normal});
    }

    std::vector<vertex> make_cube() {
        constexpr float h = 0.5f;
        std::vector<vertex> vertices;
        vertices.reserve(36);
        add_face(vertices, {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}, {0, 0, 1});
        add_face(vertices, { h, -h, -h}, {-h, -h, -h}, {-h,  h, -h}, { h,  h, -h}, {0, 0, -1});
        add_face(vertices, {-h, -h, -h}, {-h, -h,  h}, {-h,  h,  h}, {-h,  h, -h}, {-1, 0, 0});
        add_face(vertices, { h, -h,  h}, { h, -h, -h}, { h,  h, -h}, { h,  h,  h}, {1, 0, 0});
        add_face(vertices, {-h,  h,  h}, { h,  h,  h}, { h,  h, -h}, {-h,  h, -h}, {0, 1, 0});
        add_face(vertices, {-h, -h, -h}, { h, -h, -h}, { h, -h,  h}, {-h, -h,  h}, {0, -1, 0});
        return vertices;
    }

    vkz::buffer create_vertex_buffer(vkz::vma_memory_allocator& allocator, const std::vector<vertex>& vertices) {
        auto buffer =
            vkz::buffer::builder(allocator)
                .usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
                .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .size(sizeof(vertex) * vertices.size())
                .build();
        auto mapping = buffer.map();
        std::memcpy(mapping.as<vertex>(), vertices.data(), sizeof(vertex) * vertices.size());
        mapping.unmap();
        return buffer;
    }
}

int main() {
    vkz::iostream_adapter::install(std::cout);

    if (!glfwInit()) {
        VKZ_THROW("Failed to initialize GLFW")
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "vulkanizer CSM test", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        VKZ_THROW("Failed to create GLFW window")
    }

    uint32_t required_extension_count{};
    const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_extension_count);
    if (!required_extensions) {
        glfwDestroyWindow(window);
        glfwTerminate();
        VKZ_THROW("GLFW could not provide Vulkan instance extensions")
    }

    glfw_surface_provider surface_provider{window};
    VkPhysicalDeviceSynchronization2Features synchronization2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
    synchronization2.synchronization2 = VK_TRUE;
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
    dynamic_rendering.dynamicRendering = VK_TRUE;
    VkPhysicalDeviceVulkan11Features vulkan11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    vulkan11.multiview = VK_TRUE;

    auto builder = vkz::context::builder();
    builder
        .app_name("vulkanizer CSM test")
        .engine_name("vulkanizer")
        .api_version(VK_API_VERSION_1_3)
        .surface(surface_provider)
        .add_extension(synchronization2)
        .add_extension(dynamic_rendering)
        .add_extension(vulkan11)
        .add_device_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    for (uint32_t i = 0; i < required_extension_count; ++i) {
        builder.add_instance_extension(required_extensions[i]);
    }

    auto context = builder.build();
    auto allocator = vkz::vma_memory_allocator::create(context);
    const auto queue_family_index = find_graphics_present_queue_family(context.device.physical, context.surface);

    VkQueue graphics_queue{};
    vkGetDeviceQueue(context.device.logical, queue_family_index, 0, &graphics_queue);

    auto create_swapchain = [&] {
        return std::make_unique<vkz::swapchain>(
            vkz::swapchain::builder(context)
                .set_image_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                .build());
    };

    auto swapchain = create_swapchain();
    auto swapchain_image_views = create_swapchain_image_views(context.device.logical, *swapchain);

    VkCommandPoolCreateInfo command_pool_create_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_create_info.queueFamilyIndex = queue_family_index;

    VkCommandPool command_pool{};
    VKZ_CHECK_VULKAN(vkCreateCommandPool(context.device.logical, &command_pool_create_info, nullptr, &command_pool));

    auto depth_image =
        vkz::image::builder(allocator)
            .format(context.depth_format)
            .extent(swapchain->width(), swapchain->height())
            .usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            .build();
    auto depth_view =
        vkz::image_view::builder(context.device.logical)
            .image(depth_image)
            .view_type(VK_IMAGE_VIEW_TYPE_2D)
            .format(context.depth_format)
            .aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
            .level_count(1)
            .layer_count(1)
            .build();

    VkRenderPass render_pass = create_render_pass(context.device.logical, swapchain->format(), context.depth_format);
    auto framebuffers = create_framebuffers(
        context.device.logical,
        render_pass,
        swapchain_image_views,
        depth_view.handle,
        swapchain->width(),
        swapchain->height());

    VkDescriptorPoolSize descriptor_pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32},
    };
    VkDescriptorPoolCreateInfo descriptor_pool_create_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descriptor_pool_create_info.maxSets = 16;
    descriptor_pool_create_info.poolSizeCount = 2;
    descriptor_pool_create_info.pPoolSizes = descriptor_pool_sizes;

    VkDescriptorPool descriptor_pool{};
    VKZ_CHECK_VULKAN(vkCreateDescriptorPool(context.device.logical, &descriptor_pool_create_info, nullptr, &descriptor_pool));

    vkz::imgui::init({
        .window = window,
        .vulkan_context = &context,
        .queue_family = queue_family_index,
        .queue = graphics_queue,
        .min_image_count = 2,
        .image_count = swapchain->image_count(),
        .api_version = VK_API_VERSION_1_3,
        .color_attachment_format = swapchain->format(),
    });

    const auto plane_vertices = make_plane(40.0f);
    const auto cube_vertices = make_cube();
    auto plane_buffer = create_vertex_buffer(allocator, plane_vertices);
    auto cube_buffer = create_vertex_buffer(allocator, cube_vertices);

    std::vector<object> objects;
    objects.push_back({plane_buffer, static_cast<uint32_t>(plane_vertices.size()), glm::mat4{1}});

    std::mt19937 rng{1337};
    std::uniform_real_distribution<float> position{-14.0f, 14.0f};
    std::uniform_real_distribution<float> size{0.6f, 2.5f};
    std::uniform_real_distribution<float> rotation{0.0f, glm::two_pi<float>()};
    for (int i = 0; i < 42; ++i) {
        const auto scale = glm::vec3{size(rng), size(rng) * 1.5f, size(rng)};
        glm::mat4 transform{1};
        transform = glm::translate(transform, {position(rng), scale.y * 0.5f, position(rng)});
        transform = glm::rotate(transform, rotation(rng), {0.0f, 1.0f, 0.0f});
        transform = glm::scale(transform, scale);
        objects.push_back({cube_buffer, static_cast<uint32_t>(cube_vertices.size()), transform});
    }

    auto initial_transition_command_buffer = begin_command_buffer(context.device.logical, command_pool);
    const auto csm_id = vkz::csm::create({
        .device = context.device,
        .depth_format = context.depth_format,
        .memory_allocator = allocator,
        .vertex_shader_include = R"(
layout(location = 0) in vec3 position;
mat4 get_model_matrix() {
    return mat4(1.0);
}
)",
        .vertex_shader_position_offset = offsetof(vertex, position),
        .vertex_shader_position_stride = sizeof(vertex),
        .descriptor_pool = descriptor_pool,
        .initial_transition_command_buffer = initial_transition_command_buffer,
        .debug_render_pass = render_pass,
        .debug_resolution = {swapchain->width(), swapchain->height()},
        .in_flight_frames = max_frames_in_flight,
        .num_cascades = 4,
        .size = 2048,
    });
    submit_and_free(context.device.logical, graphics_queue, command_pool, initial_transition_command_buffer);

    auto scene_descriptor_set_layout =
        vkz::descriptor_set_layout_builder{context.device}
            .name("csm_test_scene_descriptor_set_layout")
            .binding(0)
                .descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                .descriptor_count(1)
                .shader_stages(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .binding(1)
                .descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                .descriptor_count(1)
                .shader_stages(VK_SHADER_STAGE_FRAGMENT_BIT)
            .binding(2)
                .descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                .descriptor_count(1)
                .shader_stages(VK_SHADER_STAGE_FRAGMENT_BIT)
            .create_layout();

    auto scene_buffer =
        vkz::buffer::builder(allocator)
            .usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .size(sizeof(scene_data))
            .build();
    auto scene_mapping = scene_buffer.map();
    auto* scene_cpu = scene_mapping.as<scene_data>();

    std::vector<float> split_depth(vkz::csm::cascade_count(csm_id));
    auto split_buffer =
        vkz::buffer::builder(allocator)
            .usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .size(sizeof(float) * split_depth.size())
            .build();
    auto split_mapping = split_buffer.map();
    auto* split_cpu = split_mapping.as<float>();

    VkDescriptorSetAllocateInfo scene_descriptor_allocate_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    scene_descriptor_allocate_info.descriptorPool = descriptor_pool;
    scene_descriptor_allocate_info.descriptorSetCount = 1;
    scene_descriptor_allocate_info.pSetLayouts = &scene_descriptor_set_layout;

    VkDescriptorSet scene_descriptor_set{};
    VKZ_CHECK_VULKAN(vkAllocateDescriptorSets(context.device.logical, &scene_descriptor_allocate_info, &scene_descriptor_set));

    VkDescriptorBufferInfo scene_buffer_info{scene_buffer, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo cascade_buffer_info{vkz::csm::cascade_view_projection(csm_id), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo split_buffer_info{split_buffer, 0, VK_WHOLE_SIZE};
    std::array<VkWriteDescriptorSet, 3> scene_writes{};
    for (auto& write : scene_writes) {
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = scene_descriptor_set;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    scene_writes[0].dstBinding = 0;
    scene_writes[0].pBufferInfo = &scene_buffer_info;
    scene_writes[1].dstBinding = 1;
    scene_writes[1].pBufferInfo = &cascade_buffer_info;
    scene_writes[2].dstBinding = 2;
    scene_writes[2].pBufferInfo = &split_buffer_info;
    vkUpdateDescriptorSets(context.device.logical, static_cast<uint32_t>(scene_writes.size()), scene_writes.data(), 0, nullptr);

    VkPipelineLayout scene_pipeline_layout{};
    auto scene_pipeline =
        vkz::graphics_pipeline_builder{context.device}
            .shader_stage()
                .vertex_shader(shader_path("csm_scene.vert"))
                .fragment_shader(shader_path("csm_scene.frag"))
            .vertex_input_state()
                .add_vertex_binding_description(0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX)
                .add_vertex_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, position))
                .add_vertex_attribute_description(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, normal))
            .input_assembly_state()
                .triangles()
            .viewport_state()
                .viewport()
                    .origin(0, 0)
                    .dimension(swapchain->width(), swapchain->height())
                    .min_depth(0.0f)
                    .max_depth(1.0f)
                .scissor()
                    .offset(0, 0)
                    .extent(static_cast<int32_t>(swapchain->width()), static_cast<int32_t>(swapchain->height()))
            .rasterization_state()
                .cull_back_face()
                .front_face_counter_clockwise()
                .polygon_mode_fill()
            .depth_stencil_state()
                .enable_depth_test()
                .enable_depth_write()
                .compare_op_less_or_equal()
            .color_blend_state()
                .attachment()
                    .add()
            .layout()
                .add_descriptor_set_layout(vkz::csm::descriptor_set_layout(csm_id))
                .add_descriptor_set_layout(scene_descriptor_set_layout)
                .add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(render_push_constants))
            .render_pass(render_pass)
            .name("csm_test_scene")
            .build(scene_pipeline_layout);

    VkSemaphoreCreateInfo semaphore_create_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore image_available{};
    VkSemaphore render_finished{};
    VKZ_CHECK_VULKAN(vkCreateSemaphore(context.device.logical, &semaphore_create_info, nullptr, &image_available));
    VKZ_CHECK_VULKAN(vkCreateSemaphore(context.device.logical, &semaphore_create_info, nullptr, &render_finished));

    VkFenceCreateInfo fence_create_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence frame_fence{};
    VKZ_CHECK_VULKAN(vkCreateFence(context.device.logical, &fence_create_info, nullptr, &frame_fence));

    bool freeze_shadow_map = false;
    bool freeze_pressed = false;
    bool show_shadow_map = false;
    bool color_cascades = false;
    bool use_pcf_filtering = true;
    bool show_extents = false;
    bool color_shadow = false;
    bool auto_light = true;
    float split_lambda = vkz::csm::DEFAULT_CASCADE_SLIT_LAMBDA;
    float light_angle = 0.35f;
    float camera_angle = 0.65f;
    uint32_t current_frame = 0;
    double previous_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        wait_for_drawable_window(window);
        if (glfwWindowShouldClose(window)) {
            break;
        }

        uint32_t image_index{};
        const auto acquire_result = vkAcquireNextImageKHR(
            context.device.logical,
            swapchain->handle(),
            UINT64_MAX,
            image_available,
            {},
            &image_index);
        VKZ_CHECK_VULKAN(acquire_result);

        vkz::imgui::new_frame();
        ImGui::Begin("CSM");
        ImGui::SetWindowSize({0, 0});
        if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            freeze_pressed = ImGui::Checkbox("freeze shadow", &freeze_shadow_map);
            if (!freeze_shadow_map) {
                ImGui::SliderFloat("Split lambda", &split_lambda, 0.0f, 1.0f);
            }
            ImGui::Checkbox("Color cascades", &color_cascades);
            if (color_cascades) {
                ImGui::SameLine();
                ImGui::Checkbox("Show extents", &show_extents);
            }
            ImGui::Checkbox("Color shadow", &color_shadow);
            ImGui::Checkbox("PCF filtering", &use_pcf_filtering);
            ImGui::Checkbox("Show depth map", &show_shadow_map);
            ImGui::Checkbox("Auto light", &auto_light);
            if (!auto_light) {
                ImGui::SliderFloat("Light angle", &light_angle, 0.0f, glm::two_pi<float>());
            }
            ImGui::SliderFloat("Camera angle", &camera_angle, 0.0f, glm::two_pi<float>());
        }
        ImGui::End();

        const double time = glfwGetTime();
        const float delta_time = static_cast<float>(time - previous_time);
        previous_time = time;
        if (auto_light) {
            light_angle += delta_time * 0.65f;
            if (light_angle > glm::two_pi<float>()) {
                light_angle -= glm::two_pi<float>();
            }
        }

        const glm::vec3 eye{std::cos(camera_angle) * 22.0f, 13.0f, std::sin(camera_angle) * 22.0f};
        const auto view = glm::lookAt(eye, glm::vec3{0.0f, 2.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
        const auto projection = vkz::perspective(glm::radians(60.0f), static_cast<float>(swapchain->width()) / swapchain->height(), 0.1f, 80.0f);
        const auto view_projection = projection * view;
        const glm::vec3 light_direction = glm::normalize(glm::vec3{std::cos(light_angle) * 0.7f, 0.8f, std::sin(light_angle) * 0.7f});

        *scene_cpu = {
            .view = view,
            .light_dir = {light_direction, 0.0f},
            .num_cascades = static_cast<int>(vkz::csm::cascade_count(csm_id)),
            .use_pcf = use_pcf_filtering ? 1 : 0,
            .color_cascades = color_cascades ? 1 : 0,
            .show_extents = show_extents ? 1 : 0,
            .color_shadow = color_shadow ? 1 : 0,
            .camera_frozen = freeze_shadow_map ? 1 : 0,
        };

        if (!freeze_shadow_map || freeze_pressed) {
            vkz::csm::split_lambda(csm_id, split_lambda);
            vkz::csm::update(
                csm_id,
                {.view_projection = view_projection, .near_plane = 0.1f, .far_plane = 80.0f},
                light_direction,
                split_depth);
            std::memcpy(split_cpu, split_depth.data(), sizeof(float) * split_depth.size());
        }

        auto command_buffer = begin_command_buffer(context.device.logical, command_pool);

        if (!freeze_shadow_map) {
            vkz::csm::capture(csm_id, [&](VkPipelineLayout layout) {
                for (const auto& item : objects) {
                    const VkDeviceSize offset = 0;
                    shadow_push_constants constants{.world = item.transform};
                    vkCmdPushConstants(command_buffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
                    vkCmdBindVertexBuffers(command_buffer, 0, 1, &item.vertices, &offset);
                    vkCmdDraw(command_buffer, item.vertex_count, 1, 0, 0);
                }
            }, command_buffer, static_cast<int>(current_frame));
        }

        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = {0.06f, 0.08f, 0.10f, 1.0f};
        clear_values[1].depthStencil = {1.0f, 0u};

        VkRenderPassBeginInfo render_pass_begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        render_pass_begin.renderPass = render_pass;
        render_pass_begin.framebuffer = framebuffers[image_index];
        render_pass_begin.renderArea.extent = {swapchain->width(), swapchain->height()};
        render_pass_begin.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_begin.pClearValues = clear_values.data();
        vkCmdBeginRenderPass(command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

        if (show_shadow_map) {
            vkz::csm::render(csm_id, command_buffer);
        } else {
            std::array<VkDescriptorSet, 2> descriptor_sets{vkz::csm::descriptor_set(csm_id), scene_descriptor_set};
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scene_pipeline);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scene_pipeline_layout, 0, static_cast<uint32_t>(descriptor_sets.size()), descriptor_sets.data(), 0, nullptr);
            for (const auto& item : objects) {
                const VkDeviceSize offset = 0;
                render_push_constants constants{.world = item.transform, .view_projection = view_projection};
                vkCmdPushConstants(command_buffer, scene_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
                vkCmdBindVertexBuffers(command_buffer, 0, 1, &item.vertices, &offset);
                vkCmdDraw(command_buffer, item.vertex_count, 1, 0, 0);
            }
        }

        vkz::imgui::render(command_buffer);
        vkCmdEndRenderPass(command_buffer);

        submit_and_free(context.device.logical, graphics_queue, command_pool, command_buffer, image_available, render_finished, frame_fence);

        VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        const auto swapchain_handle = swapchain->handle();
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &render_finished;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain_handle;
        present_info.pImageIndices = &image_index;
        VKZ_CHECK_VULKAN(vkQueuePresentKHR(graphics_queue, &present_info));

        current_frame = (current_frame + 1) % max_frames_in_flight;
    }

    vkDeviceWaitIdle(context.device.logical);

    vkDestroySemaphore(context.device.logical, render_finished, nullptr);
    vkDestroySemaphore(context.device.logical, image_available, nullptr);
    vkDestroyFence(context.device.logical, frame_fence, nullptr);
    vkDestroyPipeline(context.device.logical, scene_pipeline, nullptr);
    vkDestroyPipelineLayout(context.device.logical, scene_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(context.device.logical, scene_descriptor_set_layout, nullptr);
    scene_mapping.unmap();
    split_mapping.unmap();
    allocator.deallocate(scene_buffer);
    allocator.deallocate(split_buffer);
    vkz::csm::destroy(csm_id);
    allocator.deallocate(cube_buffer);
    allocator.deallocate(plane_buffer);
    vkz::imgui::destroy();
    vkDestroyDescriptorPool(context.device.logical, descriptor_pool, nullptr);
    destroy_framebuffers(context.device.logical, framebuffers);
    vkDestroyRenderPass(context.device.logical, render_pass, nullptr);
    vkDestroyImageView(context.device.logical, depth_view.handle, nullptr);
    allocator.deallocate(depth_image);
    destroy_image_views(context.device.logical, swapchain_image_views);
    swapchain.reset();
    vkDestroyCommandPool(context.device.logical, command_pool, nullptr);
    allocator.destroy();
    context = {};
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
