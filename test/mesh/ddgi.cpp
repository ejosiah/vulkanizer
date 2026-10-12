#define VKZ_IOSTREAM_ADAPTER
#include <vulkanizer/ddgi.hpp>
#include <vulkanizer/shadow.hpp>
#include <vulkanizer/reflection.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkanizer/vulkan_app.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/barrier.hpp>
#include <glm/gtc/packing.hpp>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
    vkz::vulkan_app_create_info create_info() {
        vkz::vulkan_app_create_info info{.width = 32, .height = 32, .title = "DDGI verification", .validation = true,
            .enabled_features = {.shader_storage_image_extended_formats = true}};
        info.device_extensions = {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
        VkPhysicalDeviceBufferDeviceAddressFeatures address{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES}; address.bufferDeviceAddress = VK_TRUE;
        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR}; acceleration.accelerationStructure = VK_TRUE;
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR tracing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR}; tracing.rayTracingPipeline = VK_TRUE;
        VkPhysicalDeviceDescriptorIndexingFeatures indexing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES}; indexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        VkPhysicalDeviceScalarBlockLayoutFeatures scalar{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES}; scalar.scalarBlockLayout = VK_TRUE;
        VkPhysicalDeviceRayQueryFeaturesKHR ray_query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
        ray_query.rayQuery = VK_TRUE;
        info.extension_chain.add(ray_query).add(address).add(acceleration).add(tracing).add(indexing).add(scalar);
        return info;
    }
    vkz::mesh::data room() {
        vkz::mesh::data scene;
        scene.streams.bindings = {{0, 56, VK_VERTEX_INPUT_RATE_VERTEX}};
        scene.streams.attributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 24}, {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 40}};
        for (int axis = 0; axis < 3; ++axis) for (float sign : {-1.0f, 1.0f}) {
            glm::vec3 normal(0), tangent(0); normal[axis] = sign; tangent[(axis + 1) % 3] = 1;
            const auto bitangent = glm::cross(normal, tangent);
            const auto first = uint32_t(scene.vertex_data.size() / 56);
            for (const glm::vec2 corner : {glm::vec2{-1, -1}, glm::vec2{1, -1}, glm::vec2{1, 1}, glm::vec2{-1, 1}}) {
                vkz::mesh::put(scene.vertex_data, normal * 2.0f + (tangent * corner.x + bitangent * corner.y) * 2.0f);
                vkz::mesh::put(scene.vertex_data, normal);
                vkz::mesh::put(scene.vertex_data, glm::vec4(tangent, 1));
                vkz::mesh::put(scene.vertex_data, glm::vec4(1));
            }
            for (auto index : {0u, 1u, 2u, 0u, 2u, 3u}) scene.index_data.push_back(first + index);
        }
        vkz::mesh::base mesh; mesh.vertex_count = 24; mesh.lod_offset[1] = 36;
        scene.meshes.push_back(mesh); scene.instances.emplace_back(); scene.materials.emplace_back();
        scene.materials[0].metallic_factor = 0; scene.materials[0].double_sided = true;
        vkz::mesh::recalculate_bounding_boxes(scene);
        return scene;
    }
    class fixture {
    public:
        fixture() : app(create_info()), allocator(vkz::vma_memory_allocator::create(app.context(), VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT)), acceleration(allocator), scene(room()) {
            glfwHideWindow(app.window());
            const auto geometry_usage = vkz::buffer::usage::acceleration_storage_input;
            geometry.vertices = upload(geometry_usage, scene.vertex_data.data(), scene.vertex_data.size());
            geometry.indices = upload(geometry_usage, scene.index_data.data(), scene.index_data.size() * sizeof(uint32_t));
            depth = target(VK_FORMAT_R32_SFLOAT); normal = target(VK_FORMAT_R16G16_SFLOAT);
            vkz::command_pool commands{allocator.device, app.queue_family_index(), vkz::command_pool_flags::transient, app.graphics_queue()};
            auto cmd = commands.create_command_buffer();
            acceleration.build(cmd, scene, geometry);
            vkz::clear(cmd, depth.image, VkClearColorValue{{0.5, 0, 0, 0}}, {});
            vkz::clear(cmd, normal.image, VkClearColorValue{{0, 1, 0, 0}}, {});
            for (auto *value : {&depth, &normal})
                vkz::barrier::push_and_flush(cmd, value->image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1}, vkz::pipeline::stage::transfer,
                                             vkz::pipeline::stage::compute_shader, vkz::access::transfer_write, vkz::access::shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            commands.submit_and_wait(cmd);
            lights.push_back({.type = vkz::mesh::light_type::point, .intensity = 20, .range = 10, .position = {0, 0, 0}});
        }
        ~fixture() {
            vkDeviceWaitIdle(allocator.device); acceleration.destroy();
            geometry.vertices.destroy(); geometry.indices.destroy(); depth.destroy(); normal.destroy(); allocator.destroy();
        }
        void run(bool half_resolution, glm::vec3 surface_color = glm::vec3(1)) {
            scene.materials[0].base_color_factor = glm::vec4(surface_color, 1);
            lights[0].intensity = 20;
            vkz::ddgi gi(allocator.device, allocator, lights, depth, normal, {17, 13});
            vkz::ddgi::settings settings;
            settings.probe_grid.count = {2, 2, 2}; settings.probe_grid.rays_per_probe = 64;
            settings.grid_position = {-0.5f, -0.5f, -0.5f}; settings.visibility_side_length = 4;
            settings.half_resolution = half_resolution; settings.relocate_probes = true; settings.classify_probes = true;
            settings.infinite_bounces = false; settings.hysteresis = 0;
            gi.configure(settings);
            gi.init({scene, geometry, acceleration.tlas().handle, app.graphics_queue(), app.queue_family_index()});
            check(gi.irradiance_atlas().image.create_info.extent.width == 32, "Irradiance atlas dimensions");
            check(gi.visibility_atlas().image.create_info.extent.width == 24, "Visibility atlas dimensions");
            check(gi.indirect_light_texture().image.create_info.extent.width == (half_resolution ? 8u : 17u), "Output resolution");
            const auto extent = gi.indirect_light_texture().image.create_info.extent;
            auto readback =
                vkz::buffer::builder(allocator)
                    .size(VkDeviceSize(extent.width) * extent.height * 8)
                    .usage(vkz::buffer::usage::transfer_dst)
                    .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
                    .build();
            vkz::command_pool commands{allocator.device, app.queue_family_index(), vkz::command_pool_flags::transient, app.graphics_queue()};
            for (int frame = 0; frame < 3; ++frame) {
                if (frame == 2) lights[0].intensity = 0;
                gi.new_frame({glm::mat4(1), {0, 0, 1}});
                auto cmd = commands.create_command_buffer();
                gi.exec(cmd);
                const VkImageSubresourceRange range{static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1};
                auto output = gi.indirect_light_texture().image;
                vkz::barrier::push_and_flush(cmd, output, range, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::transfer, vkz::access::memory_read_write, vkz::access::transfer_read,
                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                VkBufferImageCopy copy{};
                copy.imageSubresource = {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 0, 1};
                copy.imageExtent = extent;
                vkCmdCopyImageToBuffer(cmd, output, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
                vkz::barrier::push_and_flush(cmd, output, range, vkz::pipeline::stage::transfer, vkz::pipeline::stage::all_commands, vkz::access::transfer_read, vkz::access::shader_read_write,
                                             VK_IMAGE_LAYOUT_GENERAL);
                vkz::barrier::gpu_to_cpu(cmd, {readback});
                commands.submit_and_wait(cmd);
                VKZ_CHECK_VULKAN(vmaInvalidateAllocation(readback.allocator, readback.allocation, 0, VK_WHOLE_SIZE));
                auto mapping = readback.map(); const auto *pixels = mapping.as<uint16_t>();
                float total = 0;
                glm::vec3 channel_totals(0);
                for (uint32_t i = 0; i < extent.width * extent.height; ++i) {
                    const auto rg = glm::unpackHalf2x16(uint32_t(pixels[i * 4]) | uint32_t(pixels[i * 4 + 1]) << 16);
                    check(std::isfinite(rg.x) && std::isfinite(rg.y), "DDGI output must be finite");
                    const auto ba = glm::unpackHalf2x16(uint32_t(pixels[i * 4 + 2]) | uint32_t(pixels[i * 4 + 3]) << 16);
                    check(std::isfinite(ba.x), "DDGI blue channel must be finite");
                    channel_totals += glm::vec3(rg, ba.x);
                    total += rg.x + rg.y + ba.x;
                }
                if (frame < 2) check(total > 0.1f, "DDGI must produce indirect light from the explicit point light"); else check(total < 0.001f, "Removing light intensity must remove indirect light");
                if (frame < 2) {
                    for (int channel = 0; channel < 3; ++channel) {
                        if (surface_color[channel] == 0)
                            check(channel_totals[channel] < 0.001f, "Indirect irradiance must preserve the reflecting surface color");
                        else
                            check(channel_totals[channel] > 0.1f, "A colored reflecting surface must contribute indirect irradiance");
                    }
                }

                mapping.unmap(); gi.end_frame();
            }
            readback.destroy();
            std::cout << "Passed DDGI " << (half_resolution ? "half" : "full") << " resolution\n";
        }
        void run_shadows(float alpha = -1) {
            auto white = target(VK_FORMAT_R8G8B8A8_UNORM);
            white.sampler = vkz::sampler::builder(allocator.device).build();
            {
                vkz::command_pool uploads{allocator.device, app.queue_family_index(), vkz::command_pool_flags::transient, app.graphics_queue()};
                auto cmd = uploads.create_command_buffer();
                vkz::clear(cmd, white.image, VkClearColorValue{{1, 1, 1, 1}}, {});
                vkz::barrier::push_and_flush(cmd, white.image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1}, vkz::pipeline::stage::transfer,
                                             vkz::pipeline::stage::all_commands, vkz::access::transfer_write, vkz::access::shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                if (alpha >= 0) {
                    scene.materials[0].alpha_mode = alpha == 0.5f ? vkz::mesh::alpha_mode::blend : vkz::mesh::alpha_mode::mask;
                    scene.materials[0].base_color_factor.a = alpha;
                    acceleration.update_instances(cmd, scene);
                }
                uploads.submit_and_wait(cmd);
            }
            const std::array textures{white};

            lights[0].position = {0, alpha >= 0 ? 4.0f : 1.5f, 0.5f};
            vkz::shadow shadows(allocator.device, allocator, lights, depth, normal, {17, 13});
            shadows.init({acceleration.tlas().handle, app.graphics_queue(), app.queue_family_index(), alpha >= 0 ? &scene : nullptr, geometry, textures});

            const auto extent = shadows.visibility_texture().image.create_info.extent;
            check(extent.width == 9 && extent.height == 7, "Shadow half resolution must cover odd dimensions");

            auto readback =
                vkz::buffer::builder(allocator)
                    .size(VkDeviceSize(extent.width) * extent.height * 2)
                    .usage(vkz::buffer::usage::transfer_dst)
                    .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
                    .build();

            vkz::command_pool commands{allocator.device, app.queue_family_index(), vkz::command_pool_flags::transient, app.graphics_queue()};

            for (int frame = 0; frame < 12; ++frame) {
                // Move the light beyond the room ceiling; all rays must become blocked.
                if (frame == 6)
                    lights[0].position.y = 4;

                const auto view_projection = glm::translate(glm::mat4(1), glm::vec3(float(frame) * 0.01f, 0, 0));
                shadows.new_frame({glm::inverse(view_projection), view_projection, {0, 0, 1}});
                const auto cmd = commands.create_command_buffer();
                shadows.exec(cmd);

                auto output = shadows.visibility_texture().image;
                const VkImageSubresourceRange range{static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1};
                vkz::barrier::push_and_flush(cmd, output, range, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::transfer, vkz::access::memory_read_write, vkz::access::transfer_read,
                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                vkz::copy(cmd, output, readback);

                vkz::barrier::push_and_flush(cmd, output, range, vkz::pipeline::stage::transfer, vkz::pipeline::stage::compute_shader, vkz::access::transfer_read, vkz::access::shader_read_write,
                                             VK_IMAGE_LAYOUT_GENERAL);
                vkz::barrier::gpu_to_cpu(cmd, {readback});
                commands.submit_and_wait(cmd);

                VKZ_CHECK_VULKAN(vmaInvalidateAllocation(readback.allocator, readback.allocation, 0, VK_WHOLE_SIZE));
                auto mapping = readback.map();
                const auto *pixels = mapping.as<uint16_t>();
                float total = 0;

                for (uint32_t i = 0; i < extent.width * extent.height; ++i) {
                    const float value = glm::unpackHalf1x16(pixels[i]);
                    check(std::isfinite(value) && value >= 0 && value <= 1, "Shadow visibility must be finite and bounded");
                    total += value;
                }

                if (frame == 5 && alpha < 0)
                    check(total > extent.width * extent.height * 0.95f, "Unobstructed light must be visible");

                if (frame == 11 && alpha < 0)
                    check(total < extent.width * extent.height * 0.05f, "Ceiling must block the light after history updates");

                if (frame == 11 && alpha >= 0) {
                    const float mean = total / float(extent.width * extent.height);
                    check(std::abs(mean - (1 - alpha)) < 0.05f, "Masked and blended shadow coverage must match alpha");
                    std::cout << "Alpha " << alpha << " shadow visibility: " << mean << '\n';
                }

                mapping.unmap();
                shadows.end_frame();
            }

            readback.destroy();
            std::cout << "Passed shadow visibility, temporal updates and odd half resolution\n";
            white.destroy();
            scene.materials[0].alpha_mode = vkz::mesh::alpha_mode::opaque;
            scene.materials[0].base_color_factor.a = 1;
            const auto restore = commands.create_command_buffer();
            acceleration.update_instances(restore, scene);
            commands.submit_and_wait(restore);
        }

        void run_reflections() {
            scene.materials[0].base_color_factor = glm::vec4(1);
            scene.materials[0].emissive_factor = {2000, 0, 0};
            lights[0].intensity = 0;
            auto surface = target(VK_FORMAT_R16G16B16A16_SFLOAT);
            auto mesh_id = target(VK_FORMAT_R32_UINT);
            auto white = target(VK_FORMAT_R8G8B8A8_UNORM);
            white.sampler =
                vkz::sampler::builder(allocator.device)
                .build();

            vkz::command_pool commands{allocator.device, app.queue_family_index(), vkz::command_pool_flags::transient, app.graphics_queue()};
            auto cmd = commands.create_command_buffer();
            vkz::clear(cmd, surface.image, VkClearColorValue{{0.8f, 0.8f, 0.8f, 0}}, {});
            VkClearColorValue id{};
            id.uint32[0] = 1;
            vkz::clear(cmd, mesh_id.image, id, {});
            vkz::clear(cmd, white.image, VkClearColorValue{{1, 1, 1, 1}}, {});

            for (auto *value : {&surface, &mesh_id, &white}) {
                vkz::barrier::push_and_flush(cmd, value->image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1}, vkz::pipeline::stage::transfer,
                                             vkz::pipeline::stage::all_commands, vkz::access::transfer_write, vkz::access::shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            commands.submit_and_wait(cmd);

            {
                vkz::ddgi gi(allocator.device, allocator, lights, depth, normal, {17, 13});
                vkz::ddgi::settings settings;
                settings.probe_grid.count = {2, 2, 2};
                settings.probe_grid.rays_per_probe = 32;
                settings.grid_position = {-0.5f, -0.5f, -0.5f};
                settings.hysteresis = 0;
                settings.infinite_bounces = false;
                gi.configure(settings);
                gi.init({scene, geometry, acceleration.tlas().handle, app.graphics_queue(), app.queue_family_index()});
                gi.new_frame({glm::mat4(1), {0, 4, 0.5f}});
                cmd = commands.create_command_buffer();
                gi.exec(cmd);
                commands.submit_and_wait(cmd);
                gi.end_frame();

                const std::array textures{white};
                vkz::reflection reflections(allocator.device, allocator, lights, depth, normal, surface, mesh_id, {17, 13}, gi);
                reflections.init({scene, geometry, acceleration.tlas().handle, app.graphics_queue(), app.queue_family_index(), textures});
                const auto extent = reflections.reflection_texture().image.create_info.extent;
                check(extent.width == 9 && extent.height == 7, "Reflection half resolution must cover odd dimensions");
                auto readback =
                    vkz::buffer::builder(allocator)
                        .size(VkDeviceSize(extent.width) * extent.height * 8)
                        .usage(vkz::buffer::usage::transfer_dst)
                        .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
                        .build();

                for (int frame = 0; frame < 7; ++frame) {
                    const bool enabled = frame < 6;
                    const float offset = float(frame) * 0.02f;
                    const auto view_projection = glm::translate(glm::mat4(1), glm::vec3(offset, 0, 0));
                    reflections.new_frame({glm::inverse(view_projection), view_projection, {0, 4, 0.5f}, enabled});
                    cmd = commands.create_command_buffer();
                    reflections.exec(cmd);
                    auto output = reflections.reflection_texture().image;
                    vkz::copy(cmd, output, readback);
                    vkz::barrier::push_and_flush(cmd, output, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1}, vkz::pipeline::stage::transfer,
                                                 vkz::pipeline::stage::all_commands, vkz::access::transfer_read, vkz::access::shader_read_write, VK_IMAGE_LAYOUT_GENERAL);
                    vkz::barrier::gpu_to_cpu(cmd, {readback});
                    commands.submit_and_wait(cmd);
                    VKZ_CHECK_VULKAN(vmaInvalidateAllocation(readback.allocator, readback.allocation, 0, VK_WHOLE_SIZE));
                    auto mapping = readback.map();
                    const auto *pixels = mapping.as<uint16_t>();
                    glm::vec3 totals(0);

                    for (uint32_t i = 0; i < extent.width * extent.height; ++i) {
                        for (int channel = 0; channel < 4; ++channel) {
                            const float value = glm::unpackHalf1x16(pixels[i * 4 + channel]);
                            check(std::isfinite(value) && value >= 0, "Reflection must remain finite during camera movement");
                            if (channel < 3)
                                totals[channel] += value;
                        }
                    }

                    if (enabled) {
                        check(totals.x > 1, "Mirror must reflect the emissive room");
                        check(totals.y < 0.001f && totals.z < 0.001f, "Reflection must retain emitter color");
                    } else {
                        check(glm::length(totals) < 0.001f, "Disabled reflections must discard temporal history");
                    }

                    mapping.unmap();
                    reflections.end_frame();
                }

                readback.destroy();
            }

            surface.destroy();
            mesh_id.destroy();
            white.destroy();
            std::cout << "Passed emissive reflections, moving-camera history and disable reset\n";
        }

    private:
      vkz::buffer upload(vkz::buffer::usage usage, const void *data, size_t size) {
          auto result = vkz::buffer::builder(allocator).size(size).usage(usage).memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU).build();
          auto mapping = result.map();
          std::memcpy(mapping.as<void>(), data, size);
          result.flush();
          mapping.unmap();
          return result;
      }
        vkz::texture target(VkFormat format) {
            vkz::texture result;
            result.image = vkz::image::builder(allocator)
                .format(format)
                .extent(17, 13)
                .usage(vkz::image::usage::sampled_upload)
                .build();
            result.image_view = vkz::image_view::builder(allocator.device)
                .image(result.image)
                .format(format)
                .aspect_mask(vkz::image::aspect::color)
                .level_count(1)
                .layer_count(1)
                .build();
            return result;
        }
        vkz::vulkan_app app;
        vkz::vma_memory_allocator allocator;
        vkz::mesh::bvh acceleration;
        vkz::mesh::data scene;
        vkz::mesh::geometry_buffers geometry;
        vkz::texture depth, normal;
        std::vector<vkz::mesh::light> lights;
    };
}
int main() try {
    vkz::iostream_adapter::install(std::cout);
    fixture test; test.run(false, {1, 0, 0}); test.run(false, {0, 0, 1}); test.run(true); test.run_shadows(); test.run_shadows(0); test.run_shadows(0.5f); test.run_shadows(1); test.run_reflections(); return 0;
} catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
