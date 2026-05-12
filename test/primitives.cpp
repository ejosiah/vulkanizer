#define VKZ_IOSTREAM_ADAPTER

#include "vulkan_app.hpp"

#include <vulkanizer/barrier.hpp>
#include <vulkanizer/graphics_pipeline_builder.hpp>
#include <vulkanizer/imgui.hpp>
#include <vulkanizer/io.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/memory.hpp>
#include <vulkanizer/primitives.hpp>
#include <vulkanizer/render.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/transforms.hpp>

#include <imgui.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace {
    constexpr uint32_t window_width = 1280;
    constexpr uint32_t window_height = 800;

    struct push_constants {
        glm::mat4 model{1};
        glm::mat4 view_projection{1};
    };

    struct mesh {
        vkz::buffer vertex_buffer{};
        vkz::buffer index_buffer{};
        uint32_t index_count{};
    };

    enum class primitive_choice {
        cube,
        plane,
        sphere,
        hemisphere,
        cone,
        cylinder,
        torus,
        implicit,
    };

    enum class implicit_choice {
        paraboloid,
        cone,
        ripple,
    };

    constexpr std::array primitive_labels{
        "Cube",
        "Plane",
        "Sphere",
        "Hemisphere",
        "Cone",
        "Cylinder",
        "Torus",
        "Implicit",
    };

    constexpr std::array implicit_labels{
        "x^2 + y^2",
        "sqrt(x^2 + y^2)",
        "sin((pi / 2) * sqrt(x^2 + y^2))",
    };

    std::string shader_path(const char* name) {
        return std::string{VKZ_PRIMITIVE_TEST_SHADER_DIR} + "/" + name + ".spv";
    }

    float implicit_paraboloid(float x, float y) {
        return x * x + y * y;
    }

    float implicit_cone(float x, float y) {
        return std::sqrt(x * x + y * y);
    }

    float implicit_ripple(float x, float y) {
        return 0.3f * std::sin(glm::two_pi<float>() * std::sqrt(x * x + y * y));
    }

    using implicit_function = float (*)(float, float);

    implicit_function selected_implicit_function(implicit_choice choice) {
        switch (choice) {
            case implicit_choice::paraboloid:
                return implicit_paraboloid;
            case implicit_choice::cone:
                return implicit_cone;
            case implicit_choice::ripple:
                return implicit_ripple;
        }

        return implicit_paraboloid;
    }

    vkz::prim::primitive make_primitive(primitive_choice primitive, implicit_choice implicit) {
        using enum vkz::prim::topology;
        constexpr glm::vec4 color{0.55f, 0.72f, 0.42f, 1.0f};

        switch (primitive) {
            case primitive_choice::cube:
                return vkz::prim::cube(color);
            case primitive_choice::plane:
                return vkz::prim::plane(1, 1, 4.0f, 4.0f, glm::mat4{1}, color, TRIANGLES);
            case primitive_choice::sphere:
                return vkz::prim::sphere(48, 96, 1.4f, glm::mat4{1}, color, TRIANGLES);
            case primitive_choice::hemisphere:
                return vkz::prim::hemisphere(48, 96, 1.4f, color, TRIANGLES);
            case primitive_choice::cone:
                return vkz::prim::cone(48, 96, 1.0f, 2.2f, color, TRIANGLES);
            case primitive_choice::cylinder:
                return vkz::prim::cylinder(48, 96, 1.0f, 2.2f, color, TRIANGLES);
            case primitive_choice::torus:
                return vkz::prim::torus(48, 96, 0.65f, 0.32f, glm::mat4{1}, color, TRIANGLES);
            case primitive_choice::implicit:
                return vkz::prim::implicit(
                    96,
                    96,
                    4.0f,
                    4.0f,
                    selected_implicit_function(implicit),
                    glm::mat4{1},
                    color,
                    TRIANGLES);
        }

        return vkz::prim::cube(color);
    }

    vkz::buffer create_buffer(
            vkz::vma_memory_allocator& allocator,
            VkBufferUsageFlags usage,
            const void* data,
            size_t size) {
        auto buffer =
            vkz::buffer::builder(allocator)
                .usage(usage)
                .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .size(size)
                .build();
        auto mapping = buffer.map();
        std::memcpy(mapping.as<void>(), data, size);
        mapping.unmap();
        return buffer;
    }

    mesh create_mesh(vkz::vma_memory_allocator& allocator, const vkz::prim::primitive& primitive) {
        mesh result{};
        result.vertex_buffer = create_buffer(
            allocator,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            primitive.vertices.data(),
            sizeof(vkz::prim::vertex) * primitive.vertices.size());
        result.index_buffer = create_buffer(
            allocator,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            primitive.indices.data(),
            sizeof(vkz::uint) * primitive.indices.size());
        result.index_count = static_cast<uint32_t>(primitive.indices.size());
        return result;
    }

    void destroy_mesh(vkz::vma_memory_allocator& allocator, mesh& mesh) {
        if (mesh.vertex_buffer._) {
            allocator.deallocate(mesh.vertex_buffer);
        }
        if (mesh.index_buffer._) {
            allocator.deallocate(mesh.index_buffer);
        }
        mesh = {};
    }
}

int main() {
    vkz::iostream_adapter::install(std::cout);

    vkz::test::vulkan_app app{{window_width, window_height, "vulkanizer primitive test"}};
    auto& context = app.context();
    auto* window = app.window();
    const auto queue_family_index = app.queue_family_index();
    const auto graphics_queue = app.graphics_queue();
    auto allocator = vkz::vma_memory_allocator::create(context);

    auto swapchain = app.create_swapchain();
    auto swapchain_image_views = vkz::test::create_swapchain_image_views(context.device.logical, *swapchain);
    const auto depth_format = vkz::test::pick_depth_format(context.device.physical);

    auto depth_image =
        vkz::image::builder(allocator)
            .format(depth_format)
            .extent(swapchain->width(), swapchain->height())
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

    VkPipelineLayout pipeline_layout{};
    auto pipeline =
        vkz::graphics_pipeline_builder{context.device}
            .shader_stage()
                .vertex_shader(shader_path("primitive.vert"))
                .fragment_shader(shader_path("primitive.frag"))
            .vertex_input_state()
                .add_vertex_binding_description(0, sizeof(vkz::prim::vertex), VK_VERTEX_INPUT_RATE_VERTEX)
                .add_vertex_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkz::prim::vertex, position))
                .add_vertex_attribute_description(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkz::prim::vertex, normal))
                .add_vertex_attribute_description(2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vkz::prim::vertex, color))
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
                .cull_none()
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
                .add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants))
            .dynamic_render_pass()
                .add_color_attachment(swapchain->format())
                .depth_attachment(depth_format)
            .name("primitive_test")
            .build(pipeline_layout);

    VkCommandPool command_pool = vkz::test::create_command_pool(context.device.logical, queue_family_index);
    VkSemaphore image_available = vkz::test::create_semaphore(context.device.logical);
    VkSemaphore render_finished = vkz::test::create_semaphore(context.device.logical);
    VkFence frame_fence = vkz::test::create_fence(context.device.logical);

    auto primitive_selection = primitive_choice::cube;
    auto implicit_selection = implicit_choice::paraboloid;
    auto current_mesh = create_mesh(allocator, make_primitive(primitive_selection, implicit_selection));
    float camera_angle = 0.65f;
    float camera_distance = 6.0f;

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
        ImGui::Begin("Primitive");
        ImGui::SetWindowSize({0, 0});

        int primitive_index = static_cast<int>(primitive_selection);
        if (ImGui::Combo("Primitive", &primitive_index, primitive_labels.data(), static_cast<int>(primitive_labels.size()))) {
            primitive_selection = static_cast<primitive_choice>(primitive_index);
            vkDeviceWaitIdle(context.device.logical);
            destroy_mesh(allocator, current_mesh);
            current_mesh = create_mesh(allocator, make_primitive(primitive_selection, implicit_selection));
        }

        if (primitive_selection == primitive_choice::implicit) {
            int implicit_index = static_cast<int>(implicit_selection);
            if (ImGui::Combo("Function", &implicit_index, implicit_labels.data(), static_cast<int>(implicit_labels.size()))) {
                implicit_selection = static_cast<implicit_choice>(implicit_index);
                vkDeviceWaitIdle(context.device.logical);
                destroy_mesh(allocator, current_mesh);
                current_mesh = create_mesh(allocator, make_primitive(primitive_selection, implicit_selection));
            }
        }

        ImGui::SliderFloat("Camera angle", &camera_angle, 0.0f, glm::two_pi<float>());
        ImGui::SliderFloat("Distance", &camera_distance, 3.0f, 12.0f);
        ImGui::End();

        auto command_buffer = vkz::test::begin_command_buffer(context.device.logical, command_pool);
        auto image = swapchain->get_image(image_index);

        VkImageSubresourceRange color_range{};
        color_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        color_range.levelCount = 1;
        color_range.layerCount = 1;
        vkz::barrier::push_and_flush(
            command_buffer,
            image,
            color_range,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkImageSubresourceRange depth_range{};
        depth_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_range.levelCount = 1;
        depth_range.layerCount = 1;
        VkImage depth_handle = depth_image;
        vkz::barrier::push_and_flush(
            command_buffer,
            depth_handle,
            depth_range,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        vkz::render_info render_info{};
        render_info.color_attachments.push_back({
            .view = swapchain_image_views[image_index],
            .format = swapchain->format(),
            .clear_value = {0.06f, 0.08f, 0.10f, 1.0f},
        });
        render_info.depth_attachment = vkz::depth_stencil_attachment{
            .view = depth_view,
            .format = depth_format,
            .clear_value = {1.0f, 0.0f},
        };
        render_info.render_area = {swapchain->width(), swapchain->height()};

        const glm::vec3 eye{
            std::cos(camera_angle) * camera_distance,
            3.0f,
            std::sin(camera_angle) * camera_distance,
        };
        const auto view = glm::lookAt(eye, glm::vec3{0.0f, 0.25f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
        const auto projection = vkz::perspective(
            glm::radians(55.0f),
            static_cast<float>(swapchain->width()) / static_cast<float>(swapchain->height()),
            0.1f,
            60.0f);
        const push_constants constants{
            .model = glm::mat4{1},
            .view_projection = projection * view,
        };

        vkz::render(command_buffer, render_info, [&] {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
            const VkDeviceSize offset = 0;
            VkBuffer vertex_buffer = current_mesh.vertex_buffer;
            vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &offset);
            vkCmdBindIndexBuffer(command_buffer, current_mesh.index_buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(command_buffer, current_mesh.index_count, 1, 0, 0, 0);
            vkz::imgui::render(command_buffer);
        });

        vkz::barrier::push_and_flush(
            command_buffer,
            image,
            color_range,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkz::test::submit_and_free(
            context.device.logical,
            graphics_queue,
            command_pool,
            command_buffer,
            image_available,
            render_finished,
            frame_fence);

        VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        const auto swapchain_handle = swapchain->handle();
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &render_finished;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain_handle;
        present_info.pImageIndices = &image_index;
        VKZ_CHECK_VULKAN(vkQueuePresentKHR(graphics_queue, &present_info));
    }

    vkDeviceWaitIdle(context.device.logical);

    destroy_mesh(allocator, current_mesh);
    vkDestroySemaphore(context.device.logical, render_finished, nullptr);
    vkDestroySemaphore(context.device.logical, image_available, nullptr);
    vkDestroyFence(context.device.logical, frame_fence, nullptr);
    vkDestroyCommandPool(context.device.logical, command_pool, nullptr);
    vkDestroyPipeline(context.device.logical, pipeline, nullptr);
    vkDestroyPipelineLayout(context.device.logical, pipeline_layout, nullptr);
    vkz::imgui::destroy();
    vkDestroyImageView(context.device.logical, depth_view.handle, nullptr);
    allocator.deallocate(depth_image);
    vkz::test::destroy_image_views(context.device.logical, swapchain_image_views);
    swapchain.reset();
    allocator.destroy();
    return 0;
}
