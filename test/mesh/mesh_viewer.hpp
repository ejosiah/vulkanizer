#pragma once
#define VKZ_IOSTREAM_ADAPTER
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "../timeout.hpp"
#include <vulkanizer/vulkan_app.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/glfw_input_adaptor.hpp>
#include <vulkanizer/graphics_pipeline_builder.hpp>
#include <vulkanizer/mesh.hpp>
#include <vulkanizer/render.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/log.hpp>
#include <algorithm>
#include <charconv>
#include <cstring>
#include <cmath>
#include <iostream>
#include <optional>
#include "gltf_reference.hpp"
#include "shader_resources.hpp"

namespace reference_test {
    struct scene_options {
        vkz::path mesh;
        const char *name;
        bool orbit{};
        vkz::path environment;
        std::optional<glm::vec3> eye;
        std::optional<glm::vec3> target;
        uint32_t width{1280};
        uint32_t height{800};
    };

    namespace {
        struct push_constants {
            glm::mat4 model{1};
            glm::mat4 view_projection{1};
        };

        vkz::vulkan_app_create_info create_info(bool validation, bool wireframe, bool ray_traced, bool ray_query, const char *title, uint32_t width, uint32_t height) {
            vkz::vulkan_app_create_info info{.width = width, .height = height, .title = title, .resizable = false, .validation = validation, .enabled_features = {.fill_mode_non_solid = wireframe}};
            if (ray_traced || ray_query) {
                info.device_extensions = {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
                VkPhysicalDeviceBufferDeviceAddressFeatures address{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
                address.bufferDeviceAddress = VK_TRUE;
                VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
                acceleration.accelerationStructure = VK_TRUE;
                VkPhysicalDeviceRayTracingPipelineFeaturesKHR tracing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
                tracing.rayTracingPipeline = VK_TRUE;
                VkPhysicalDeviceDescriptorIndexingFeatures indexing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
                indexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
                info.extension_chain.add(address).add(acceleration).add(indexing);
                if (ray_traced) {
                    info.device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
                    info.extension_chain.add(tracing);
                }
                if (ray_query) {
                    info.device_extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
                    VkPhysicalDeviceRayQueryFeaturesKHR query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
                    query.rayQuery = VK_TRUE;
                    info.extension_chain.add(query);
                }
            }
            return info;
        }

        vkz::sample_count select_samples(VkPhysicalDevice device, VkFormat color_format, VkFormat depth_format, uint32_t requested) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);
            VkImageFormatProperties color{}, depth{};
            VKZ_CHECK_VULKAN(vkGetPhysicalDeviceImageFormatProperties(device, color_format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, static_cast<VkImageUsageFlags>(vkz::image_usage_color), 0, &color));
            VKZ_CHECK_VULKAN(vkGetPhysicalDeviceImageFormatProperties(device, depth_format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, static_cast<VkImageUsageFlags>(vkz::image_usage_depth_stencil), 0, &depth));
            const auto supported = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts & color.sampleCounts & depth.sampleCounts;
            uint32_t selected = requested;
            while (selected > 1 && !(supported & selected))
                selected >>= 1;
            std::cout << "Raster samples: " << selected;
            if (selected != requested) std::cout << " (requested " << requested << ", limited by attachment support)";
            std::cout << '\n';
            return static_cast<vkz::sample_count>(selected);
        }

        vkz::buffer upload(vkz::vma_memory_allocator &allocator, vkz::buffer_usage usage, const void *bytes, size_t size) {
            auto result = vkz::buffer::builder(allocator)
                .usage(usage)
                .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .size(size)
                .build();
            auto mapping = result.map();
            std::memcpy(mapping.as<void>(), bytes, size);
            VKZ_CHECK_VULKAN(vmaFlushAllocation(result.allocator, result.allocation, 0, VK_WHOLE_SIZE));
            mapping.unmap();
            return result;
        }
    }

    int run(int argc, char **argv, const scene_options &options) try {
        vkz::iostream_adapter::install(std::cout);
        std::string screenshot;
        bool ibl = !options.environment.empty();
        bool blur = false;
        bool validation = false;
        bool wireframe = false;
        bool ray_traced = false;
        uint32_t samples = 8;
        bool samples_set = false;
        float exposure = 0;
        for (int i = 1; i < argc; ++i) {
            if (std::string_view{argv[i]} == "--blur") blur = true;
            if (std::string_view{argv[i]} == "--no-ibl") ibl = false;
            if (std::string_view{argv[i]} == "--validation") validation = true;
            if (std::string_view{argv[i]} == "--wireframe") wireframe = true;
            if (std::string_view{argv[i]} == "--ray-traced") ray_traced = true;
            if (std::string_view{argv[i]} == "--exposure") {
                if (++i == argc) VKZ_THROW("--exposure requires a number of stops from -16 to 16");
                const std::string_view value{argv[i]};
                const auto result = std::from_chars(value.data(), value.data() + value.size(), exposure);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || !std::isfinite(exposure) || exposure < -16 || exposure > 16)
                    VKZ_THROW("--exposure must be a finite number of stops from -16 to 16");
            }
            if (std::string_view{argv[i]} == "--samples") {
                samples_set = true;
                if (++i == argc) VKZ_THROW("--samples requires a count (ray tracing: 1-32; raster: 1, 2, 4, 8, 16, 32, 64)");
                const std::string_view value{argv[i]};
                const auto result = std::from_chars(value.data(), value.data() + value.size(), samples);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || samples < 1 || samples > 64) VKZ_THROW("--samples must be an integer from 1 to 64");
            }
            if (std::string_view{argv[i]} == "--screenshot") {
                if (++i == argc) throw std::invalid_argument("--screenshot requires a PNG path");
                screenshot = argv[i];
            }
        }
        if (ray_traced && wireframe) VKZ_THROW("Choose either --ray-traced or --wireframe");
        if (ray_traced && samples > 32) VKZ_THROW("Ray-traced --samples must be from 1 to 32");
        if (!ray_traced && samples_set && (samples & (samples - 1))) VKZ_THROW("Raster --samples must be 1, 2, 4, 8, 16, 32 or 64");
        vkz::mesh::data scene;
        vkz::mesh::load_mesh_data(options.mesh, scene);
        if (scene.instances.empty()) throw std::runtime_error("Reference scene has no mesh instances");
        auto material_path = vkz::path{options.mesh};
        material_path.replace_extension(".vkmat");
        vkz::mesh::load_mesh_data_materials(material_path, scene);
        auto bounds = scene.boxes.at(scene.instances.front().mesh_id).get_transformed(scene.instances.front().model);
        for (const auto &instance : scene.instances) {
            const auto box = scene.boxes.at(instance.mesh_id).get_transformed(instance.model);
            bounds.combine_point(box.min);
            bounds.combine_point(box.max);
        }
        const auto title = std::string(options.name) + (ray_traced ? " ray tracing reference" : " graphics reference") +
                           (options.orbit ? " - hold left mouse to orbit, wheel to zoom" : " - WASD, hold left mouse to look");
        const bool ray_query = !wireframe && !ray_traced && std::any_of(scene.materials.begin(), scene.materials.end(), [](const auto &material) {
            return material.transmission_factor > 0;
        });
        vkz::vulkan_app app(create_info(validation, wireframe, ray_traced, ray_query, title.c_str(), options.width, options.height));
        auto &context = app.context();
        const auto device = context.device;
        const auto queue = app.graphics_queue();
        auto allocator = vkz::vma_memory_allocator::create(context, (ray_traced || ray_query) ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT : 0);
        auto swapchain = app.create_swapchain(vkz::image_usage_color_download);
        auto views = vkz::create_swapchain_image_views(device, *swapchain);
        const auto width = swapchain->width(), height = swapchain->height();
        const vkz::buffer_usage ray_usage = (ray_traced || ray_query) ? vkz::buffer_usage_acceleration_input : vkz::buffer_usage_none;
        auto vertices = upload(allocator, vkz::buffer_usage_vertex_storage | ray_usage, scene.vertex_data.data(), scene.vertex_data.size());
        auto indices = upload(allocator, vkz::buffer_usage_index_storage | ray_usage, scene.index_data.data(), scene.index_data.size() * sizeof(uint32_t));
        const auto depth_format = vkz::pick_depth_format(context.device.physical);
        const auto raster_samples =
            ray_traced ? vkz::sample_count_x1 : select_samples(device.physical, wireframe ? swapchain->format() : VK_FORMAT_R32G32B32A32_SFLOAT, depth_format, samples_set ? samples : 1);
        auto wireframe_color = make_multisample_color(allocator, {width, height}, swapchain->format(), wireframe ? raster_samples : vkz::sample_count_x1);
        auto depth = vkz::image::builder(allocator)
            .format(depth_format)
            .extent(width, height)
            .samples(raster_samples)
            .usage(vkz::image_usage_depth_stencil)
            .build();
        auto depth_view =
            vkz::image_view::builder(device)
                .image(depth)
                .view_type(VK_IMAGE_VIEW_TYPE_2D)
                .format(depth_format)
                .aspect_mask(vkz::image_aspect_depth)
                .level_count(1)
                .layer_count(1)
                .build();
        auto readback = vkz::buffer::builder(allocator)
            .usage(vkz::buffer_usage_transfer_dst)
            .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
            .size(VkDeviceSize(width) * height * 4)
            .build();
        const auto &position = scene.streams.attributes.at(0);
        vkz::pipeline pipeline;
        if (wireframe) {
            pipeline = vkz::graphics_pipeline_builder{device}
                           .shader_stage()
                           .vertex_shader(reference::shader("sponza.vert.spv"))
                           .fragment_shader(reference::shader("sponza.frag.spv"))
                           .vertex_input_state()
                           .add_vertex_binding_description(0, scene.streams.bindings.at(0).stride, VK_VERTEX_INPUT_RATE_VERTEX)
                           .add_vertex_attribute_description(0, 0, position.format, position.offset)
                           .input_assembly_state()
                           .triangles()
                           .viewport_state()
                           .viewport()
                           .origin(0, 0)
                           .dimension(width, height)
                           .min_depth(0)
                           .max_depth(1)
                           .scissor()
                           .offset(0, 0)
                           .extent(width, height)
                           .add()
                           .rasterization_state()
                           .cull_none()
                           .front_face_counter_clockwise()
                           .polygon_mode_line()
                           .multisample_state()
                           .rasterization_samples(raster_samples)
                           .depth_stencil_state()
                           .enable_depth_test()
                           .enable_depth_write()
                           .compare_op_less_or_equal()
                           .color_blend_state()
                           .attachment()
                           .add()
                           .layout()
                           .add_push_constant_range(vkz::shader_stage_vertex, 0, sizeof(push_constants))
                           .dynamic_render_pass()
                           .add_color_attachment(swapchain->format())
                           .depth_attachment(depth_format)
                           .name("mesh_wireframe")
                           .build();
        }

        std::unique_ptr<reference_test::renderer> reference_renderer;
        if (!wireframe)
            reference_renderer = std::make_unique<reference_test::renderer>(allocator, queue, app.queue_family_index(), scene, vertices, width, height, swapchain->format(), depth_format, indices,
                                                                            ray_traced, samples, ibl ? options.environment : vkz::path{}, raster_samples);
        scene.images.clear();
        scene.images.shrink_to_fit();
        vkz::glfw_input_adaptor input(app.window(), false);
        input.bind();
        vkz::camera::camera camera;
        const auto center = bounds.get_center();
        camera.position = {center.x + bounds.get_size().x * 0.3f, bounds.min.y + 1.7f, center.z};
        camera.acceleration = {18, 18, 18};
        camera.velocity = {8, 8, 8};
        camera.rotationSpeed = 0.15f;
        camera.horizontal_fov = true;
        if (options.orbit) {
            const float radius = glm::length(bounds.get_size()) * 0.5f;
            const float half_horizontal = glm::radians(65.0f) * 0.5f;
            const float half_vertical = std::atan(std::tan(half_horizontal) * float(height) / width);
            const float distance = radius / std::sin(std::min(half_horizontal, half_vertical)) * 1.05f;
            camera.position = center + glm::vec3{0, 0, distance};
            camera.minZoom = radius * 0.2f;
            camera.maxZoom = distance * 10;
            vkz::camera::orbit initializer(camera);
            initializer.look_at(camera.position, center, {0, 1, 0});
            initializer.perspective(65, float(width) / height, radius * 0.01f, distance * 100);
        } else {
            if (options.eye) camera.position = *options.eye;
            vkz::camera::first_person initializer(camera);
            initializer.look_at(camera.position, options.target.value_or(glm::vec3{center.x, camera.position.y, center.z}), {0, 1, 0});
            initializer.perspective(65, float(width) / height, 0.05f, 1000);
        }
        vkz::camera::controller controller(camera, options.orbit ? vkz::camera::movement_type::orbit : vkz::camera::movement_type::first_person, input.get_device());
        vkz::fenced_command_pools commands{device, queue, app.queue_family_index(), vkz::command_pool_flags_transient, 1};
        auto available = vkz::create_semaphore(device);
        std::vector<VkSemaphore> finished(swapchain->image_count());
        for (auto &semaphore : finished)
            semaphore = vkz::create_semaphore(device);
        uint32_t frame = 0;
        auto previous = std::chrono::steady_clock::now();
        test_timeout timeout(argc, argv);
        while (!app.should_close() && !timeout.expired()) {
            app.poll_events();
            if (glfwGetKey(app.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
            app.wait_for_drawable_window();
            if (app.should_close()) break;
            const auto now = std::chrono::steady_clock::now();
            controller.process_input();
            controller.update(std::min(0.1f, std::chrono::duration<float>(now - previous).count()));
            previous = now;
            commands.set_cycle_and_wait(frame);
            const reference_test::frame_info reference_frame{
                .view = camera.view, .projection = camera.projection, .eye = camera.position, .light_direction = glm::normalize(camera.zAxis + glm::vec3(0, 0.5f, 0)), .blur = blur};
            if (reference_renderer) reference_renderer->update(scene, reference_frame);
            uint32_t index;
            auto result = vkAcquireNextImageKHR(device, swapchain->handle(), UINT64_MAX, available, {}, &index);
            if (result == VK_ERROR_OUT_OF_DATE_KHR) break;
            if (result != VK_SUBOPTIMAL_KHR) VKZ_CHECK_VULKAN(result);
            auto cmd = commands.create_command_buffer();
            VkImage color = swapchain->get_image(index);
            VkImageSubresourceRange color_range{static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1};
            VkImageSubresourceRange depth_range{static_cast<VkImageAspectFlags>(vkz::image_aspect_depth), 0, 1, 0, 1};
            const auto output_stage = vkz::pipeline_stage_color_attachment_output;
            const auto output_access = vkz::access_color_attachment_write;
            const auto output_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            vkz::barrier::push_and_flush(cmd, color, color_range, output_stage, output_stage, vkz::access_none, output_access, VK_IMAGE_LAYOUT_UNDEFINED, output_layout);
            if (ray_traced) {
                reference_renderer->trace(cmd);
            } else {
                const auto depth_stages = vkz::pipeline_stage_early_fragment_tests | vkz::pipeline_stage_late_fragment_tests;
                const bool initialized = depth.layout != VK_IMAGE_LAYOUT_UNDEFINED;
                vkz::barrier::push_and_flush(cmd, depth, depth_range, initialized ? depth_stages : vkz::pipeline_stage_none, depth_stages,
                                             initialized ? vkz::access_depth_stencil_attachment_write : vkz::access_none, vkz::access_depth_stencil_attachment_read_write,
                                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
                if (reference_renderer) reference_renderer->prepare_color(cmd);
                if (wireframe && raster_samples != vkz::sample_count_x1)
                    vkz::barrier::push_and_flush(cmd, wireframe_color.image, color_range, vkz::pipeline_stage_all_commands, vkz::pipeline_stage_color_attachment_output,
                                                 vkz::access_memory_read_write, vkz::access_color_attachment_write, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                vkz::render_info rendering{};
                rendering.color_attachments.push_back({.view = reference_renderer                        ? reference_renderer->color_view()
                                                               : raster_samples != vkz::sample_count_x1 ? wireframe_color.image_view
                                                                                                         : views[index],
                                                       .format = reference_renderer ? VK_FORMAT_R32G32B32A32_SFLOAT : swapchain->format(),
                                                       .clear_value = {0, 0, 1, 1},
                                                       .resolve = reference_renderer                        ? reference_renderer->resolve_view()
                                                                  : raster_samples != vkz::sample_count_x1 ? std::optional{views[index]}
                                                                                                            : std::nullopt});
                rendering.depth_attachment = vkz::depth_stencil_attachment{.view = depth_view, .format = depth_format, .clear_value = {1, 0}};
                rendering.render_area = {width, height};
                vkz::render(cmd, rendering, [&] {
                    if (wireframe) {
                        vkz::bind_pipeline(cmd, pipeline);
                        vkz::bind_vertex_buffer(cmd, vertices);
                    }
                    vkCmdBindIndexBuffer(cmd, indices, 0, VK_INDEX_TYPE_UINT32);
                    if (reference_renderer)
                        reference_renderer->draw(cmd, scene, reference_frame);
                    else
                        for (const auto &instance : scene.instances) {
                            const auto &mesh = scene.meshes.at(instance.mesh_id);
                            vkz::push_constants(cmd, pipeline, push_constants{instance.model, camera.projection * camera.view}, vkz::shader_stage_vertex);
                            vkCmdDrawIndexed(cmd, mesh.get_lod_indices_count(0), 1, mesh.index_offset + mesh.lod_offset[0], static_cast<int32_t>(mesh.vertex_offset), 0);
                        }
                });
            }
            if (reference_renderer) {
                reference_renderer->prepare_tone_mapping(cmd);
                vkz::render_info presentation{};
                presentation.color_attachments.push_back({.view = views[index], .format = swapchain->format()});
                presentation.render_area = {width, height};
                vkz::render(cmd, presentation, [&] {
                    reference_renderer->tone_map(cmd, exposure);
                });
            }
            if (!screenshot.empty()) {
                vkz::barrier::push_and_flush(cmd, color, color_range, output_stage, vkz::pipeline_stage_transfer, output_access, vkz::access_transfer_read, output_layout,
                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                VkBufferImageCopy copy{};
                copy.imageSubresource = {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 0, 1};
                copy.imageExtent = {width, height, 1};
                vkCmdCopyImageToBuffer(cmd, color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
                VkMemoryBarrier2 host_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
                host_barrier.srcStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_transfer);
                host_barrier.srcAccessMask = static_cast<VkAccessFlags2>(vkz::access_transfer_write);
                host_barrier.dstStageMask = static_cast<VkPipelineStageFlags2>(vkz::pipeline_stage_host);
                host_barrier.dstAccessMask = static_cast<VkAccessFlags2>(vkz::access_host_read);
                VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
                dependency.memoryBarrierCount = 1;
                dependency.pMemoryBarriers = &host_barrier;
                vkCmdPipelineBarrier2(cmd, &dependency);
            }
            vkz::barrier::push_and_flush(cmd, color, color_range, screenshot.empty() ? output_stage : vkz::pipeline_stage_transfer, vkz::pipeline_stage_none,
                                         screenshot.empty() ? output_access : vkz::access_transfer_read, vkz::access_none, screenshot.empty() ? output_layout : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            VKZ_CHECK_VULKAN(vkEndCommandBuffer(cmd));
            commands.enqueue(cmd);
            commands.enqueue_wait(available, vkz::pipeline_stage_color_attachment_output);
            commands.enqueue_signal(finished[index]);
            VKZ_CHECK_VULKAN(commands.execute());
            const auto handle = swapchain->handle();
            VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores = &finished[index];
            present.swapchainCount = 1;
            present.pSwapchains = &handle;
            present.pImageIndices = &index;
            result = vkQueuePresentKHR(queue, &present);
            ++frame;
            if (result == VK_ERROR_OUT_OF_DATE_KHR) break;
            if (result != VK_SUBOPTIMAL_KHR) VKZ_CHECK_VULKAN(result);
        }
        VKZ_CHECK_VULKAN(vkDeviceWaitIdle(device));
        if (frame == 0) throw std::runtime_error("No frames rendered");
        if (!screenshot.empty()) {
            vmaInvalidateAllocation(readback.allocator, readback.allocation, 0, VK_WHOLE_SIZE);
            auto mapping = readback.map();
            auto *pixels = mapping.as<unsigned char>();
            if (swapchain->format() == VK_FORMAT_B8G8R8A8_SRGB || swapchain->format() == VK_FORMAT_B8G8R8A8_UNORM)
                for (size_t i = 0; i < size_t(width) * height * 4; i += 4)
                    std::swap(pixels[i], pixels[i + 2]);
            if (!stbi_write_png(screenshot.c_str(), width, height, 4, pixels, width * 4)) throw std::runtime_error("Failed to save screenshot");
            mapping.unmap();
        }
        std::cout << "Rendered " << frame << " frames with " << scene.instances.size() << " mesh instances\n";
        if (reference_renderer) reference_renderer->destroy();
        pipeline.destroy();
        vertices.destroy();
        indices.destroy();
        readback.destroy();
        wireframe_color.destroy();
        depth_view.destroy();
        depth.destroy();
        vkz::destroy_image_views(device, views);
        vkDestroySemaphore(device, available, nullptr);
        for (auto semaphore : finished)
            vkDestroySemaphore(device, semaphore, nullptr);
        swapchain.reset();
        allocator.destroy();
        return 0;
    } catch (const std::exception &error) {

        std::cerr << error.what() << '\n';
        return 1;
    }

}
