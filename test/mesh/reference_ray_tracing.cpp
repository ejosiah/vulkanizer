#define VKZ_IOSTREAM_ADAPTER
#include <vulkanizer/reference.hpp>
#include <vulkanizer/bvh.hpp>
#include <vulkanizer/vulkan_app.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/log.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <numbers>

namespace {
    constexpr uint32_t extent = 17;

    vkz::vulkan_app_create_info create_info() {
        vkz::vulkan_app_create_info info{.width = 32, .height = 32, .title = "Reference ray tracing verification", .validation = true};
        info.device_extensions = {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
        VkPhysicalDeviceBufferDeviceAddressFeatures address{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
        address.bufferDeviceAddress = VK_TRUE;
        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
        acceleration.accelerationStructure = VK_TRUE;
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR tracing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
        tracing.rayTracingPipeline = VK_TRUE;
        VkPhysicalDeviceDescriptorIndexingFeatures indexing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
        indexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        info.extension_chain.add(address).add(acceleration).add(tracing).add(indexing);
        return info;
    }

    class fixture {
      public:
        fixture() : app(create_info()), allocator(vkz::vma_memory_allocator::create(app.context(), VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT)), bvh(allocator) {
            glfwHideWindow(app.window());
            create_scene();
            create_buffers();
            create_images();
            create_descriptors();
            pipeline = vkz::reference::create_ray_tracing_pipeline({allocator, layouts, VKZ_COUNT(textures)});
            vkz::command_pool commands{allocator.device, app.queue_family_index(), vkz::command_pool_flags::transient, app.graphics_queue()};
            auto cmd = commands.create_command_buffer();
            bvh.build(cmd, scene, {vertices, indices});
            commands.submit_and_wait(cmd);
            write_acceleration();
        }

        ~fixture() {
            vkDeviceWaitIdle(allocator.device);
            pipeline.destroy();
            bvh.destroy();
            vkDestroyDescriptorPool(allocator.device, pool, nullptr);
            for (auto &layout : layouts)
                layout.destroy();
            for (auto &texture : textures)
                texture.destroy();
            output.destroy();
            for (auto *buffer : {&vertices, &indices, &scene_buffer, &material_buffer, &light_buffer, &instance_buffer, &readback})
                buffer->destroy();
            allocator.destroy();
        }

        void run() {
            auto &material = scene.materials[0];
            material.base_color_factor = {0.2f, 0.4f, 0.8f, 1};
            material.metallic_factor = 0;
            uniforms.scene.light_radiance = {};
            uniforms.scene.ambient_irradiance = glm::vec4(std::numbers::pi_v<float> / 0.96f);
            expect(render(), {0.2f, 0.4f, 0.8f}, "base color and ambient");
            scene.instances[0].material_id = 1;
            expect(render(), {0.7f, 0.1f, 0.3f}, "instance material override");
            scene.instances[0].material_id = vkz::mesh::inherit_material;
            scene.instances[0].model = glm::scale(glm::mat4{1}, {-1, 2, 1});
            scene.instances[0].model_inverse = glm::inverse(scene.instances[0].model);
            expect(render(), {0.2f, 0.4f, 0.8f}, "mirrored nonuniform instance");
            scene.instances[0].model = scene.instances[0].model_inverse = glm::mat4{1};
            material.alpha_mode = vkz::mesh::alpha_mode::mask;
            material.base_color_factor.a = 0.2f;
            expect(render(), {0.7f, 0.1f, 0.3f}, "alpha mask reveals rear instance");
            material.base_color_factor.a = 0.8f;
            expect(render(), {0.2f, 0.4f, 0.8f}, "alpha mask accepts surface");
            material.alpha_mode = vkz::mesh::alpha_mode::blend;
            material.base_color_factor.a = 0;
            expect(render(), {0.7f, 0.1f, 0.3f}, "zero alpha coverage");
            material.base_color_factor.a = 1;
            expect(render(), {0.2f, 0.4f, 0.8f}, "full alpha coverage");
            material.base_color_factor.a = 0.35f;
            auto pixels = render();
            const auto hits = std::count_if(pixels.begin(), pixels.end(), [](const auto &pixel) {
                return glm::length(glm::vec3(pixel) - glm::vec3{0.2f, 0.4f, 0.8f}) < 0.001f;
            });
            VKZ_ASSERT(hits > 60 && hits < 145, "Fractional alpha must sample surface coverage");
            std::cout << "Passed: fractional alpha coverage\n";
            material.alpha_mode = vkz::mesh::alpha_mode::opaque;
            material.base_color_factor.a = 0;
            expect(render(), {0.2f, 0.4f, 0.8f}, "opaque ignores alpha");
            material.base_color_factor.a = 1;
            material.base_color_texture.index = 0;
            expect(render(), {0.1f, 0.1f, 0.6f}, "base color texture");
            material.base_color_texture.rotation = std::numbers::pi_v<float> / 2;
            material.base_color_texture.scale = {2, 3};
            material.base_color_texture.offset = {0.2f, 0.3f};
            const auto transformed = vkz::reference::make_material_uniform(scene, material);
            VKZ_ASSERT(glm::length(transformed.uv_transform[0] - glm::vec4(0, 3, -2, 0)) < 1e-5f && transformed.uv_offset[0] == glm::vec4(0.2f, 0.3f, 0, 0), "glTF top-left UV rotation convention");
            material.base_color_texture.rotation = 0;
            material.base_color_texture.scale = {1, 1};
            material.base_color_texture.offset = {};
            material.metallic_roughness_texture.index = 1;
            material.metallic_factor = 0.8f;
            material.occlusion_texture.index = 3;
            material.occlusion_strength = 0.5f;
            material.emissive_texture.index = 4;
            material.emissive_factor = {0.4f, 0.6f, 0.8f};
            expect(render(), glm::vec3{0.1f, 0.1f, 0.6f} * 0.8f * 0.6f + glm::vec3{0.12f, 0.3f, 0.56f}, "metallic B, occlusion R and emissive channels");
            material = {};
            material.metallic_factor = 0;
            material.base_color_factor = {0.2f, 0.4f, 0.8f, 1};
            uniforms.scene.ambient_irradiance = {};
            uniforms.scene.light_radiance = {1, 1, 1, 0};
            uniforms.scene.light_direction = {0, 0, 1, 0};
            expect(render(), (glm::vec3{0.2f, 0.4f, 0.8f} * 0.96f + 0.01f) / std::numbers::pi_v<float>, "direct dielectric BRDF");
            uniforms.scene.light_radiance = {};
            scene.lights.push_back({.type = vkz::mesh::light_type::directional, .direction = {0, 0, -1}});
            const auto direct_expected = (glm::vec3{0.2f, 0.4f, 0.8f} * 0.96f + 0.01f) / std::numbers::pi_v<float>;
            expect(render(), direct_expected, "punctual directional light");
            scene.lights[0].type = vkz::mesh::light_type::point;
            scene.lights[0].position = {0, 0, 2};
            auto lit = render();
            VKZ_ASSERT(glm::length(glm::vec3(lit[extent * extent / 2]) - direct_expected / 4.0f) < 0.001f, "Point light inverse-square attenuation");
            scene.lights[0].range = 2;
            expect(render(), {0, 0, 0}, "punctual range cutoff");
            scene.lights[0].range = 0;
            scene.lights[0].type = vkz::mesh::light_type::spot;
            scene.lights[0].direction = {0, 0, 1};
            expect(render(), {0, 0, 0}, "spot cone rejects opposite direction");
            scene.lights.clear();
            material.unlit = true;
            material.emissive_strength = 50;
            expect(render(), {0.2f, 0.4f, 0.8f}, "unlit ignores emission strength and lighting");
            material.unlit = false;
            material.emissive_strength = 3;
            material.emissive_factor = {0.1f, 0.2f, 0.3f};
            expect(render(), {0.3f, 0.6f, 0.9f}, "HDR emissive strength");
            material.emissive_factor = {};
            material.emissive_strength = 1;
            material.base_color_factor = glm::vec4(1);
            material.roughness_factor = 0;
            material.ior = 1;
            material.transmission_factor = 1;
            expect(render(), {0.7f, 0.1f, 0.3f}, "IOR one transmission reaches rear geometry");
            uniforms.sampling.w = 0;
            expect(render(), {0, 0, 0}, "zero secondary bounce budget");
            uniforms.sampling.w = 1;
            material.ior = 1.5f;
            expect(render(), glm::vec3{0.7f, 0.1f, 0.3f} * 0.96f, "one secondary ray and dielectric Fresnel");
            material.transmission_texture.index = 0;
            expect(render(), glm::vec3{0.7f, 0.1f, 0.3f} * 0.48f, "linear transmission texture red channel");
            material.transmission_texture.index = -1;
            material.ior = 0;
            expect(render(), {0, 0, 0}, "infinite IOR blocks transmission");
            uniforms.sampling.w = 8;
            material.ior = 1.5f;
            material.transmission_factor = 0;
            material.roughness_factor = 1;
            material.base_color_factor = {0.2f, 0.4f, 0.8f, 1};
            uniforms.scene.light_radiance = {1, 1, 1, 0};
            material.normal_texture.index = 2;
            expect(render(), {0, 0, 0}, "triangle-derived normal map frame");
            material.normal_texture.index = -1;
            scene.instances[0].model = glm::rotate(glm::mat4{1}, std::numbers::pi_v<float>, glm::vec3{0, 1, 0});
            scene.instances[0].model_inverse = glm::inverse(scene.instances[0].model);
            expect(render(), {0.7f, 0.1f, 0.3f}, "single-sided back face culling");
            material.double_sided = true;
            expect(render(), (glm::vec3{0.2f, 0.4f, 0.8f} * 0.96f + 0.01f) / std::numbers::pi_v<float>, "double-sided normal reversal");
            material.base_color_factor = {0, 0, 0, 1};
            material.emissive_factor = {1, 0, 0};
            scene.instances[0].model = glm::translate(glm::mat4{1}, glm::vec3{0, 1, 0}) * glm::scale(glm::mat4{1}, glm::vec3{0.1f, 0.1f, 1});
            scene.instances[0].model_inverse = glm::inverse(scene.instances[0].model);
            for (uint32_t samples : {8u, 32u, 64u}) {
                uniforms.sampling.z = samples;
                const auto antialiased = render();
                const auto center = glm::vec3(antialiased[extent * extent / 2]);
                const auto expected = (glm::vec3{1, 0, 0} + glm::vec3{0.7f, 0.1f, 0.3f}) * 0.5f;
                VKZ_ASSERT(glm::length(center - expected) < 0.025f, "AA must average both sides of a silhouette edge");
            }
            uniforms.sampling.z = 1;
            std::cout << "Passed: jittered AA edge coverage and 32-sample cap\n";
            uniforms.sampling.y = 0;
            expect(render(), glm::vec3(uniforms.background), "miss background and ray mask");
            uniforms.sampling.y = 0xff;
            material.double_sided = false;
            scene.instances[0].model = scene.instances[0].model_inverse = glm::mat4{1};
            lod = 1;
            vkz::command_pool commands{allocator.device, app.queue_family_index(), vkz::command_pool_flags::transient, app.graphics_queue()};
            auto cmd = commands.create_command_buffer();
            bvh.build(cmd, scene, {vertices, indices}, {.lod = lod});
            commands.submit_and_wait(cmd);
            write_acceleration();
            expect(render(), glm::vec3(uniforms.background), "selected LOD and reversed winding");
            lod = 0;
            material = {};
            material.metallic_factor = 0;
            material.roughness_factor = 0;
            material.ior = 1;
            material.transmission_factor = 1;
            uniforms.scene.light_radiance = {};
            uniforms.scene.ambient_irradiance = {};
            scene.instances.clear();
            for (uint32_t layer = 0; layer < 8; ++layer) {
                auto model = glm::translate(glm::mat4{1}, glm::vec3{0, 0, -float(layer)});
                scene.instances.push_back({model, glm::inverse(model), 0});
            }
            auto rear_model = glm::translate(glm::mat4{1}, glm::vec3{0, 0, -7});
            scene.instances.push_back({rear_model, glm::inverse(rear_model), 1});
            cmd = commands.create_command_buffer();
            bvh.build(cmd, scene, {vertices, indices});
            commands.submit_and_wait(cmd);
            write_acceleration();
            uniforms.sampling.w = 7;
            expect(render(), {0, 0, 0}, "seven bounces cannot cross eight transmitting surfaces");
            uniforms.sampling.w = 8;
            expect(render(), {0.7f, 0.1f, 0.3f}, "eight bounces reach rear surface");
            scene.instances[7].model = glm::translate(glm::mat4{1}, glm::vec3{0, 0, -9});
            scene.instances[7].model_inverse = glm::inverse(scene.instances[7].model);
            scene.instances[8].material_id = 0;
            scene.instances[8].model = glm::translate(glm::mat4{1}, glm::vec3{0, 0, -7});
            scene.instances[8].model_inverse = glm::inverse(scene.instances[8].model);
            uniforms.sampling.w = 100;
            expect(render(), {0, 0, 0}, "secondary bounce budget clamps to eight");
            scene.instances = {{glm::mat4{1}, glm::mat4{1}, 0}, {glm::translate(glm::mat4{1}, glm::vec3{0, 0, -1}), glm::translate(glm::mat4{1}, glm::vec3{0, 0, 1}), 1}};
            scene.meshes[0].vertex_count = 6;
            scene.meshes[0].lod_offset[1] = 6;
            scene.index_data[6] = 5;
            scene.index_data[7] = 4;
            scene.index_data[8] = 3;
            for (uint32_t vertex = 6; vertex < 9; ++vertex) {
                float normal_z = -1;
                std::memcpy(scene.vertex_data.data() + (vertex * 16 + 5) * sizeof(float), &normal_z, sizeof(float));
            }
            write(vertices, scene.vertex_data.data(), scene.vertex_data.size());
            write(indices, scene.index_data.data(), scene.index_data.size());
            material.base_color_factor = {0.5f, 0.75f, 1, 1};
            material.transmission_factor = 0.8f;
            material.thickness_factor = 0.1f;
            material.attenuation_color = {0.5f, 0.25f, 1};
            material.attenuation_distance = 1;
            cmd = commands.create_command_buffer();
            bvh.build(cmd, scene, {vertices, indices});
            commands.submit_and_wait(cmd);
            write_acceleration();
            const auto boundary_color = glm::vec3{0.5f, 0.75f, 1} * 0.8f * glm::vec3{0.7f, 0.1f, 0.3f};
            expect(render(), boundary_color * material.attenuation_color, "closed volume absorption over one world unit, entry tint only");
            material.thickness_texture.index = 0;
            expect(render(), boundary_color * material.attenuation_color, "ray distance replaces baked thickness texture");
            scene.instances[0].model = glm::scale(glm::mat4{1}, glm::vec3{1, 1, 2});
            scene.instances[0].model_inverse = glm::inverse(scene.instances[0].model);
            scene.instances[1].model = glm::translate(glm::mat4{1}, glm::vec3{0, 0, -2});
            scene.instances[1].model_inverse = glm::inverse(scene.instances[1].model);
            expect(render(), boundary_color * material.attenuation_color * material.attenuation_color, "nonuniform instance scaling changes world-space absorption distance");
            material.attenuation_distance = std::numeric_limits<float>::infinity();
            expect(render(), boundary_color, "infinite attenuation distance is lossless");
            material.attenuation_distance = 1;
            material.thickness_factor = 0;
            expect(render(), boundary_color, "zero thickness restores thin-walled transmission");
            std::cout << "Reference ray tracing GPU verification passed\n";
        }

      private:
        template <class T> void write(vkz::buffer &buffer, const T *data, size_t count) {
            auto mapping = buffer.map();
            std::memcpy(mapping.as<void>(), data, count * sizeof(T));
            VKZ_CHECK_VULKAN(vmaFlushAllocation(buffer.allocator, buffer.allocation, 0, VK_WHOLE_SIZE));
            mapping.unmap();
        }

        vkz::buffer allocate(size_t size, vkz::buffer::usage usage) {
            return vkz::buffer::builder(allocator)
                .size(size)
                .usage(usage)
                .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .build();
        }

        void create_scene() {
            scene.streams.bindings = {{0, 16 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX}};
            scene.streams.attributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
                                        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12},
                                        {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 24},
                                        {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 40},
                                        {4, 0, VK_FORMAT_R32G32_SFLOAT, 56}};
            std::vector<float> data(3 * 16);
            for (int layer = 0; layer < 2; ++layer) {
                const std::array<glm::vec2, 3> positions{{{-10, -10}, {10, -10}, {0, 10}}};
                for (const auto &position : positions) {
                    const std::array<float, 16> vertex{position.x, position.y, -float(layer), 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, position.x * 0.05f + 0.5f, position.y * 0.05f + 0.5f};
                    data.insert(data.end(), vertex.begin(), vertex.end());
                }
                vkz::mesh::base mesh;
                mesh.vertex_offset = 3 + layer * 3;
                mesh.vertex_count = 3;
                mesh.index_offset = 3 + layer * 6;
                mesh.lod_count = 2;
                mesh.lod_offset = {0, 3, 6};
                mesh.material_id = layer;
                scene.meshes.push_back(mesh);
                scene.instances.push_back({glm::mat4{1}, glm::mat4{1}, uint32_t(layer)});
            }
            scene.vertex_data.resize(data.size() * sizeof(float));
            std::memcpy(scene.vertex_data.data(), data.data(), scene.vertex_data.size());
            scene.index_data = {0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 1, 2, 2, 1, 0};
            scene.materials.resize(2);
            scene.materials[1].base_color_factor = {0, 0, 0, 1};
            scene.materials[1].emissive_factor = {0.7f, 0.1f, 0.3f};
            scene.textures.resize(5);
            auto projection = glm::perspectiveRH_ZO(glm::radians(45.0f), 1.0f, 0.01f, 100.f);
            projection[1][1] *= -1;
            uniforms.scene.eye = {0, 0, 3, 1};
            uniforms.scene.view_projection = projection * glm::lookAtRH(glm::vec3{0, 0, 3}, glm::vec3{0}, glm::vec3{0, 1, 0});
            uniforms.inverse_view_projection = glm::inverse(uniforms.scene.view_projection);
            uniforms.scene.vertex_layout.x = vkz::reference::vertex_stride(scene);
            uniforms.sampling.z = 1;
            uniforms.background = {0.01f, 0.02f, 0.03f, 1};
        }

        void create_buffers() {
            constexpr auto geometry_usage = vkz::buffer::usage::acceleration_storage_input;
            vertices = allocate(scene.vertex_data.size(), geometry_usage);
            indices = allocate(VKZ_BYTE_SIZE(scene.index_data), geometry_usage);
            write(vertices, scene.vertex_data.data(), scene.vertex_data.size());
            write(indices, scene.index_data.data(), scene.index_data.size());
            scene_buffer = allocate(sizeof(uniforms), vkz::buffer::usage::uniform);
            instance_buffer = allocate(16 * sizeof(vkz::reference::ray_tracing_instance_uniform), vkz::buffer::usage::storage);
            material_buffer = allocate(scene.materials.size() * sizeof(vkz::reference::material_uniform), vkz::buffer::usage::storage);
            light_buffer = allocate(sizeof(vkz::reference::light_uniform), vkz::buffer::usage::storage);
            readback = allocate(extent * extent * sizeof(glm::vec4), vkz::buffer::usage::transfer_dst);
        }

        vkz::texture make_image(uint32_t size, vkz::image::usage usage) {
            vkz::texture result;
            result.image = vkz::image::builder(allocator)
                .format(VK_FORMAT_R32G32B32A32_SFLOAT)
                .extent(size, size)
                .usage(usage)
                .build();
            result.image_view =
                vkz::image_view::builder(allocator.device)
                    .image(result.image)
                    .format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .aspect_mask(vkz::image::aspect::color)
                    .level_count(1)
                    .layer_count(1)
                    .build();
            result.sampler = vkz::sampler::builder(allocator.device)
                .build();
            return result;
        }

        void create_images() {
            output = make_image(extent, vkz::image::usage::storage_download);
            const std::array<glm::vec4, 6> colors{{{1, 1, 1, 1}, {0.5f, 0.25f, 0.75f, 1}, {0.9f, 0.6f, 0.25f, 1}, {1, 0.5f, 0.5f, 1}, {0.2f, 0.8f, 0.9f, 1}, {0.3f, 0.5f, 0.7f, 1}}};
            vkz::command_pool commands{allocator.device, app.queue_family_index(), vkz::command_pool_flags::transient, app.graphics_queue()};
            auto cmd = commands.create_command_buffer();
            for (const auto &color : colors) {
                textures.push_back(make_image(1, vkz::image::usage::sampled_upload));
                vkz::clear(cmd, textures.back().image, VkClearColorValue{{color.x, color.y, color.z, color.w}}, {});
                vkz::barrier::push_and_flush(cmd, textures.back().image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1}, vkz::pipeline::stage::transfer,
                                             vkz::pipeline::stage::ray_tracing_shader, vkz::access::transfer_write, vkz::access::shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            commands.submit_and_wait(cmd);
        }

        void create_descriptors() {
            auto texture_binding = vkz::reference::ray_tracing_texture_binding(VKZ_COUNT(textures));
            VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            for (size_t i = 0; i < layouts.size(); ++i) {
                layouts[i].device = allocator.device;
                info.bindingCount = i == 0 ? VKZ_COUNT(vkz::reference::ray_tracing_scene_bindings) : 1;
                info.pBindings = i == 0 ? vkz::reference::ray_tracing_scene_bindings.data() : &texture_binding;
                VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(allocator.device, &info, nullptr, &layouts[i].handle));
            }
            const std::array<VkDescriptorPoolSize, 5> sizes{{{static_cast<VkDescriptorType>(vkz::descriptor::type::uniform_buffer), 1},
                                                             {static_cast<VkDescriptorType>(vkz::descriptor::type::storage_buffer), 5},
                                                             {static_cast<VkDescriptorType>(vkz::descriptor::type::acceleration_structure), 1},
                                                             {static_cast<VkDescriptorType>(vkz::descriptor::type::storage_image), 1},
                                                             {static_cast<VkDescriptorType>(vkz::descriptor::type::combined_image_sampler), VKZ_COUNT(textures)}}};
            VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool_info.maxSets = 2;
            pool_info.poolSizeCount = VKZ_COUNT(sizes);
            pool_info.pPoolSizes = sizes.data();
            VKZ_CHECK_VULKAN(vkCreateDescriptorPool(allocator.device, &pool_info, nullptr, &pool));
            const std::array handles{layouts[0].handle, layouts[1].handle};
            VkDescriptorSetAllocateInfo allocate_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocate_info.descriptorPool = pool;
            allocate_info.descriptorSetCount = 2;
            allocate_info.pSetLayouts = handles.data();
            VKZ_CHECK_VULKAN(vkAllocateDescriptorSets(allocator.device, &allocate_info, sets.data()));
            const std::array buffers{scene_buffer, vertices, indices, instance_buffer, material_buffer};
            for (uint32_t i = 0; i < buffers.size(); ++i) {
                VkDescriptorBufferInfo buffer_info{buffers[i], 0, VK_WHOLE_SIZE};
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = sets[0];
                write.dstBinding = i;
                write.descriptorType = i == 0 ? static_cast<VkDescriptorType>(vkz::descriptor::type::uniform_buffer) : static_cast<VkDescriptorType>(vkz::descriptor::type::storage_buffer);
                write.descriptorCount = 1;
                write.pBufferInfo = &buffer_info;
                vkUpdateDescriptorSets(allocator.device, 1, &write, 0, nullptr);
            }
            VkDescriptorBufferInfo light_info{light_buffer, 0, VK_WHOLE_SIZE};
            VkWriteDescriptorSet light_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            light_write.dstSet = sets[0];
            light_write.dstBinding = 7;
            light_write.descriptorCount = 1;
            light_write.descriptorType = static_cast<VkDescriptorType>(vkz::descriptor::type::storage_buffer);
            light_write.pBufferInfo = &light_info;
            vkUpdateDescriptorSets(allocator.device, 1, &light_write, 0, nullptr);
            VkDescriptorImageInfo output_info{{}, output.image_view, VK_IMAGE_LAYOUT_GENERAL};
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = sets[0];
            write.dstBinding = 6;
            write.descriptorCount = 1;
            write.descriptorType = static_cast<VkDescriptorType>(vkz::descriptor::type::storage_image);
            write.pImageInfo = &output_info;
            vkUpdateDescriptorSets(allocator.device, 1, &write, 0, nullptr);
            std::vector<VkDescriptorImageInfo> images;
            for (const auto &texture : textures)
                images.push_back({texture.sampler, texture.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            write.dstSet = sets[1];
            write.dstBinding = 0;
            write.descriptorCount = VKZ_COUNT(images);
            write.descriptorType = static_cast<VkDescriptorType>(vkz::descriptor::type::combined_image_sampler);
            write.pImageInfo = images.data();
            vkUpdateDescriptorSets(allocator.device, 1, &write, 0, nullptr);
        }

        void write_acceleration() {
            const auto handle = bvh.tlas().handle;
            VkWriteDescriptorSetAccelerationStructureKHR acceleration{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
            acceleration.accelerationStructureCount = 1;
            acceleration.pAccelerationStructures = &handle;
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.pNext = &acceleration;
            write.dstSet = sets[0];
            write.dstBinding = 5;
            write.descriptorCount = 1;
            write.descriptorType = static_cast<VkDescriptorType>(vkz::descriptor::type::acceleration_structure);
            vkUpdateDescriptorSets(allocator.device, 1, &write, 0, nullptr);
        }

        std::vector<glm::vec4> render() {
            uniforms.scene.vertex_layout.y = VKZ_COUNT(scene.lights);
            const auto lights = vkz::reference::make_light_uniforms(scene);
            write(light_buffer, lights.data(), lights.size());
            write(scene_buffer, &uniforms, 1);
            const auto instances = vkz::reference::make_ray_tracing_instances(scene, lod);
            write(instance_buffer, instances.data(), instances.size());
            std::vector<vkz::reference::material_uniform> materials;
            for (const auto &material : scene.materials)
                materials.push_back(vkz::reference::make_material_uniform(scene, material));
            write(material_buffer, materials.data(), materials.size());
            vkz::command_pool commands{allocator.device, app.queue_family_index(), vkz::command_pool_flags::transient, app.graphics_queue()};
            auto cmd = commands.create_command_buffer();
            bvh.update_instances(cmd, scene);
            const VkImageSubresourceRange range{static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1};
            vkz::barrier::push_and_flush(cmd, output.image, range, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::ray_tracing_shader, vkz::access::memory_read_write,
                                         vkz::access::shader_write, VK_IMAGE_LAYOUT_GENERAL);
            vkz::bind_pipeline(cmd, pipeline);
            vkCmdBindDescriptorSets(cmd, pipeline.bind_point, pipeline.layout, 0, VKZ_COUNT(sets), sets.data(), 0, nullptr);
            vkz::trace_ray(cmd, pipeline, extent, extent);
            vkz::barrier::push_and_flush(cmd, output.image, range, vkz::pipeline::stage::ray_tracing_shader, vkz::pipeline::stage::transfer, vkz::access::shader_write, vkz::access::transfer_read,
                                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 0, 1};
            copy.imageExtent = {extent, extent, 1};
            vkCmdCopyImageToBuffer(cmd, output.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
            vkz::barrier::push_and_flush(cmd, vkz::pipeline::stage::transfer, vkz::pipeline::stage::host, vkz::access::transfer_write, vkz::access::host_read);
            commands.submit_and_wait(cmd);
            VKZ_CHECK_VULKAN(vmaInvalidateAllocation(readback.allocator, readback.allocation, 0, VK_WHOLE_SIZE));
            auto mapping = readback.map();
            const auto *pixels = mapping.as<glm::vec4>();
            std::vector<glm::vec4> result(pixels, pixels + extent * extent);
            mapping.unmap();
            return result;
        }

        void expect(const std::vector<glm::vec4> &pixels, glm::vec3 expected, const char *name) {
            const auto actual = glm::vec3(pixels[(extent * extent) / 2]);
            if (glm::length(actual - expected) > 0.001f) {
                std::cerr << name << ": actual " << actual.x << ", " << actual.y << ", " << actual.z << '\n';
                VKZ_THROW("Reference ray tracing pixel mismatch");
            }
            for (const auto &pixel : pixels)
                VKZ_ASSERT(std::isfinite(pixel.x) && std::isfinite(pixel.y) && std::isfinite(pixel.z), "Nonfinite reference radiance");
            std::cout << "Passed: " << name << '\n';
        }

        uint32_t lod{};
        vkz::vulkan_app app;
        vkz::vma_memory_allocator allocator;
        vkz::mesh::bvh bvh;
        vkz::mesh::data scene;
        vkz::reference::ray_tracing_scene_uniform uniforms;
        vkz::buffer vertices, indices, scene_buffer, material_buffer, light_buffer, instance_buffer, readback;
        vkz::texture output;
        std::vector<vkz::texture> textures;
        std::array<vkz::descriptor_set_layout, 2> layouts{};
        std::array<VkDescriptorSet, 2> sets{};
        VkDescriptorPool pool{};
        vkz::ray_tracing_pipeline pipeline;
    };
}

int main() try {
    vkz::iostream_adapter::install(std::cout);
    fixture test;
    test.run();
    return 0;
} catch (const std::exception &error) {

    std::cerr << error.what() << '\n';
    return 1;
}
