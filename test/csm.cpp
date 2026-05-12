#define VKZ_IOSTREAM_ADAPTER

#include "vulkan_app.hpp"

#include <vulkanizer/barrier.hpp>
#include <vulkanizer/csm.hpp>
#include <vulkanizer/descriptor_set_builder.hpp>
#include <vulkanizer/graphics_pipeline_builder.hpp>
#include <vulkanizer/imgui.hpp>
#include <vulkanizer/io.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/memory.hpp>
#include <vulkanizer/primitives.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/transforms.hpp>

#include <imgui.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstddef>
#include <iostream>
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

    struct cull_mode_option {
        const char* label{};
        VkCullModeFlags value{};
    };

    struct shadow_push_constants {
        glm::mat4 world{1};
        int cascade_index{};
    };

    struct render_push_constants {
        glm::mat4 world{1};
        glm::mat4 view_projection{1};
    };

    std::string shader_path(const char* name) {
        return std::string{VKZ_CSM_TEST_SHADER_DIR} + "/" + name + ".spv";
    }

    VkSampleCountFlagBits pick_sample_count(VkPhysicalDevice physical_device) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical_device, &properties);

        const auto supported =
            properties.limits.framebufferColorSampleCounts &
            properties.limits.framebufferDepthSampleCounts;

        if (supported & VK_SAMPLE_COUNT_4_BIT) {
            return VK_SAMPLE_COUNT_4_BIT;
        }
        if (supported & VK_SAMPLE_COUNT_2_BIT) {
            return VK_SAMPLE_COUNT_2_BIT;
        }
        VKZ_THROW("The CSM demo requires multisampled color and depth attachments")
    }

    VkRenderPass create_render_pass(
            VkDevice device,
            VkFormat color_format,
            VkFormat depth_format,
            VkSampleCountFlagBits samples) {
        std::array<VkAttachmentDescription, 3> attachments{};
        attachments[0].format = color_format;
        attachments[0].samples = samples;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        attachments[1].format = depth_format;
        attachments[1].samples = samples;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments[2].format = color_format;
        attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depth_attachment{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkAttachmentReference resolve_attachment{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        subpass.pResolveAttachments = &resolve_attachment;
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
            VkImageView color_view,
            VkImageView depth_view,
            const std::vector<vkz::image_view>& resolve_views,
            uint32_t width,
            uint32_t height) {
        std::vector<VkFramebuffer> framebuffers;
        framebuffers.reserve(resolve_views.size());

        for (const auto& resolve_view : resolve_views) {
            std::array<VkImageView, 3> attachments{color_view, depth_view, resolve_view.handle};
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

    std::vector<vertex> to_vertices(const vkz::prim::primitive& primitive) {
        std::vector<vertex> vertices;

        if (primitive.indices.empty()) {
            vertices.reserve(primitive.vertices.size());
            for (const auto& source : primitive.vertices) {
                vertices.push_back({source.position, source.normal});
            }
            return vertices;
        }

        vertices.reserve(primitive.indices.size());
        for (const auto index : primitive.indices) {
            const auto& source = primitive.vertices[index];
            vertices.push_back({source.position, source.normal});
        }
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

    vkz::test::vulkan_app app{{
        .width = window_width,
        .height = window_height,
        .title = "vulkanizer CSM test",
        .multiview = true,
    }};
    auto& context = app.context();
    auto* window = app.window();
    auto allocator = vkz::vma_memory_allocator::create(context);
    const auto queue_family_index = app.queue_family_index();
    const auto graphics_queue = app.graphics_queue();
    const auto depth_format = vkz::test::pick_depth_format(context.device.physical);
    const auto sample_count = pick_sample_count(context.device.physical);

    auto swapchain = app.create_swapchain();
    auto swapchain_image_views = vkz::test::create_swapchain_image_views(context.device.logical, *swapchain);

    VkCommandPool command_pool = vkz::test::create_command_pool(
        context.device.logical,
        queue_family_index,
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    auto color_image =
        vkz::image::builder(allocator)
            .format(swapchain->format())
            .extent(swapchain->width(), swapchain->height())
            .samples(sample_count)
            .usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .build();
    auto color_view =
        vkz::image_view::builder(context.device.logical)
            .image(color_image)
            .view_type(VK_IMAGE_VIEW_TYPE_2D)
            .format(swapchain->format())
            .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
            .level_count(1)
            .layer_count(1)
            .build();

    auto depth_image =
        vkz::image::builder(allocator)
            .format(depth_format)
            .extent(swapchain->width(), swapchain->height())
            .samples(sample_count)
            .usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            .build();
    auto depth_view =
        vkz::image_view::builder(context.device.logical)
            .image(depth_image)
            .view_type(VK_IMAGE_VIEW_TYPE_2D)
            .format(depth_format)
            .aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
            .level_count(1)
            .layer_count(1)
            .build();

    VkRenderPass render_pass = create_render_pass(context.device.logical, swapchain->format(), depth_format, sample_count);
    auto framebuffers = create_framebuffers(
        context.device.logical,
        render_pass,
        color_view.handle,
        depth_view.handle,
        swapchain_image_views,
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
        .render_pass = render_pass,
        .samples = sample_count,
        .use_dynamic_rendering = false,
    });

    auto plane_transform = glm::rotate(glm::mat4{1}, -glm::half_pi<float>(), {1.0f, 0.0f, 0.0f});
    const auto plane_vertices = to_vertices(vkz::prim::plane(
        1,
        1,
        40.0f,
        40.0f,
        plane_transform,
        vkz::prim::GRAY,
        vkz::prim::topology::TRIANGLES));
    const auto cube_vertices = to_vertices(vkz::prim::cube());
    auto plane_buffer = create_vertex_buffer(allocator, plane_vertices);
    auto cube_buffer = create_vertex_buffer(allocator, cube_vertices);

    std::vector<object> objects;
    objects.push_back({plane_buffer, static_cast<uint32_t>(plane_vertices.size()), glm::mat4{1}});

    std::mt19937 rng{1337};
    std::uniform_real_distribution<float> position{-14.0f, 14.0f};
    std::uniform_real_distribution<float> size{0.6f, 2.5f};
    std::uniform_real_distribution<float> rotation{0.0f, glm::two_pi<float>()};
    std::uniform_real_distribution<float> hover{2.0f, 6.0f};
    for (int i = 0; i < 42; ++i) {
        const auto scale = glm::vec3{size(rng), size(rng) * 1.5f, size(rng)};
        const auto is_floating = i % 6 == 0;
        const auto y = is_floating ? hover(rng) : scale.y * 0.5f;
        glm::mat4 transform{1};
        transform = glm::translate(transform, {position(rng), y, position(rng)});
        transform = glm::rotate(transform, rotation(rng), {0.0f, 1.0f, 0.0f});
        transform = glm::scale(transform, scale);
        objects.push_back({cube_buffer, static_cast<uint32_t>(cube_vertices.size()), transform});
    }

    auto initial_transition_command_buffer = vkz::test::begin_command_buffer(context.device.logical, command_pool);
    const auto csm_id = vkz::csm::create({
        .device = context.device,
        .depth_format = depth_format,
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
        .debug_samples = sample_count,
        .in_flight_frames = max_frames_in_flight,
        .num_cascades = 4,
        .size = 2048,
    });
    vkz::test::submit_and_free(context.device.logical, graphics_queue, command_pool, initial_transition_command_buffer);

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
            .multisample_state()
                .rasterization_samples(sample_count)
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

    VkSemaphore image_available = vkz::test::create_semaphore(context.device.logical);
    VkSemaphore render_finished = vkz::test::create_semaphore(context.device.logical);
    VkFence frame_fence = vkz::test::create_fence(context.device.logical);

    bool freeze_shadow_map = false;
    bool freeze_pressed = false;
    bool show_shadow_map = false;
    bool color_cascades = false;
    bool use_pcf_filtering = true;
    bool show_extents = false;
    bool color_shadow = false;
    bool auto_light = true;
    int shadow_cull_mode_index = 1;
    float split_lambda = vkz::csm::DEFAULT_CASCADE_SLIT_LAMBDA;
    float light_angle = 0.35f;
    float camera_angle = 0.65f;
    uint32_t current_frame = 0;
    double previous_time = glfwGetTime();

    constexpr std::array<cull_mode_option, 4> shadow_cull_modes{{
        {"None", VK_CULL_MODE_NONE},
        {"Front", VK_CULL_MODE_FRONT_BIT},
        {"Back", VK_CULL_MODE_BACK_BIT},
        {"Front and back", VK_CULL_MODE_FRONT_AND_BACK},
    }};

    while (!app.should_close()) {
        app.poll_events();
        app.wait_for_drawable_window();
        if (app.should_close()) {
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
            if (ImGui::BeginCombo("Cull mode", shadow_cull_modes[shadow_cull_mode_index].label)) {
                for (int i = 0; i < static_cast<int>(shadow_cull_modes.size()); ++i) {
                    const bool selected = shadow_cull_mode_index == i;
                    if (ImGui::Selectable(shadow_cull_modes[i].label, selected)) {
                        shadow_cull_mode_index = i;
                        vkz::csm::cull_mode(csm_id, shadow_cull_modes[i].value);
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
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

        auto command_buffer = vkz::test::begin_command_buffer(context.device.logical, command_pool);

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

        std::array<VkClearValue, 3> clear_values{};
        clear_values[0].color = {0.06f, 0.08f, 0.10f, 1.0f};
        clear_values[1].depthStencil = {1.0f, 0u};
        clear_values[2].color = {0.06f, 0.08f, 0.10f, 1.0f};

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

        vkz::test::submit_and_free(context.device.logical, graphics_queue, command_pool, command_buffer, image_available, render_finished, frame_fence);

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
    vkDestroyImageView(context.device.logical, color_view.handle, nullptr);
    allocator.deallocate(color_image);
    vkz::test::destroy_image_views(context.device.logical, swapchain_image_views);
    swapchain.reset();
    vkDestroyCommandPool(context.device.logical, command_pool, nullptr);
    allocator.destroy();
    return 0;
}
