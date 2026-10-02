#pragma once
#include <vulkanizer/reference.hpp>
#include <vulkanizer/ibl.hpp>
#include <vulkanizer/bvh.hpp>
#include <vulkanizer/mesh.hpp>
#include <vulkanizer/ktx.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/status.hpp>
#include <algorithm>
#include <cstring>
#include <numeric>
#include <optional>

namespace reference_test {
    using namespace vkz::reference;

    struct frame_info {
        glm::mat4 view{1};
        glm::mat4 projection{1};
        glm::vec3 eye{};
        glm::vec3 light_direction{0, 1, 0};
        glm::vec3 light_radiance{3};
        glm::vec3 ambient_irradiance{0.15f};
        bool blur{};
    };

    template <class T> void write(vkz::buffer &buffer, const T &value) {
        auto mapping = buffer.map();
        std::memcpy(mapping.as<void>(), &value, sizeof(value));
        vmaFlushAllocation(buffer.allocator, buffer.allocation, 0, sizeof(value));
        mapping.unmap();
    }

    inline vkz::texture make_multisample_color(vkz::vma_memory_allocator &allocator, VkExtent2D extent, VkFormat format, VkSampleCountFlagBits samples) {
        vkz::texture result;
        if (samples == VK_SAMPLE_COUNT_1_BIT) return result;
        result.image = vkz::image::builder(allocator)
            .format(format)
            .extent(extent.width, extent.height)
            .samples(samples)
            .usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .build();
        result.image_view = vkz::image_view::builder(allocator.device)
            .image(result.image)
            .format(format)
            .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
            .level_count(1)
            .layer_count(1)
            .build();
        return result;
    }

    class renderer {
      public:
        renderer(vkz::vma_memory_allocator &allocator, VkQueue queue, uint32_t family, const vkz::mesh::data &scene, const vkz::buffer &vertices, uint32_t width, uint32_t height,
                 VkFormat color_format, VkFormat depth_format, const vkz::buffer &indices, bool ray_traced = false, uint32_t samples = 8, const vkz::path &environment_path = {},
                 VkSampleCountFlagBits raster_samples = VK_SAMPLE_COUNT_1_BIT)
            : ray_traced(ray_traced), samples(samples), raster_samples(raster_samples) {
            init(allocator, queue, family, scene, vertices, width, height, color_format, depth_format, indices, environment_path);
        }

        void update(const vkz::mesh::data &scene, const frame_info &frame) {
            scene_uniform data;
            data.view_projection = frame.projection * frame.view;
            data.eye = glm::vec4(frame.eye, 1);
            data.light_direction = glm::vec4(glm::normalize(frame.light_direction), 0);
            data.light_radiance = glm::vec4(frame.light_radiance, 0);
            data.ambient_irradiance = glm::vec4(frame.ambient_irradiance, 0);
            data.vertex_layout.x = int(vertex_stride(scene));
            data.vertex_layout.y = int(scene.lights.size());
            data.vertex_layout.z = std::any_of(scene.materials.begin(), scene.materials.end(), [](const auto &material) {
                return material.dispersion > 0 && material.thickness_factor > 0;
            });
            if (!scene.lights.empty()) data.light_radiance = glm::vec4(0);
            if (ibl_layout) {
                ibl_uniform environment_frame;
                environment_frame.inverse_view_projection = glm::inverse(data.view_projection);
                environment_frame.eye = data.eye;
                environment_frame.parameters.z = frame.blur ? 1.0f : 0.0f;
                write(ibl_buffer, environment_frame);
            }
            if (ray_traced || ray_query) {
                ray_tracing_scene_uniform ray_data;
                ray_data.scene = data;
                ray_data.sampling.z = samples;
                ray_data.inverse_view_projection = glm::inverse(data.view_projection);
                ray_data.background = {0, 0, 1, 1};
                write(scene_buffer, ray_data);
            } else {
                write(scene_buffer, data);
            }
        }

        void draw(VkCommandBuffer cmd, const vkz::mesh::data &scene, const frame_info &frame) {
            if (ibl_layout) {
                vkz::bind_pipeline(cmd, background_pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, background_pipeline.layout, 0, 1, &ibl_set, 0, nullptr);
                vkCmdDraw(cmd, 3, 1, 0, 0);
            }
            auto material_id = [&](const auto &instance) {
                return material_index(scene, instance);
            };
            std::vector<uint32_t> order(scene.instances.size());
            std::iota(order.begin(), order.end(), 0);
            std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
                auto blend = [&](uint32_t id) {
                    return scene.materials.at(material_id(scene.instances[id])).alpha_mode == vkz::mesh::alpha_mode::blend;
                };
                const bool ab = blend(a), bb = blend(b);
                if (ab != bb) return !ab;
                if (!ab) return false;
                auto distance = [&](uint32_t id) {
                    const auto &instance = scene.instances[id];
                    return -(frame.view * instance.model * glm::vec4(scene.boxes.at(instance.mesh_id).get_center(), 1)).z;
                };
                return distance(a) > distance(b);
            });
            for (auto id : order) {
                const auto &instance = scene.instances[id];
                const auto &mesh = scene.meshes.at(instance.mesh_id);
                const auto material = material_id(instance);
                const auto pipeline_id = (scene.materials.at(material).alpha_mode == vkz::mesh::alpha_mode::blend ? 2 : 0) + (glm::determinant(glm::mat3(instance.model)) < 0 ? 1 : 0);
                const auto &pipeline = pipelines[pipeline_id];
                vkz::bind_pipeline(cmd, pipeline);
                std::vector<VkDescriptorSet> sets{scene_set, material_sets.at(material)};
                if (ibl_layout) sets.push_back(ibl_set);
                if (ray_query) sets.push_back(query_set);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, VKZ_COUNT(sets), sets.data(), 0, nullptr);
                vkz::push_constants(cmd, pipeline, instance_uniform{instance.model, glm::transpose(instance.model_inverse)}, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
                vkCmdDrawIndexed(cmd, mesh.get_lod_indices_count(0), 1, mesh.index_offset + mesh.lod_offset[0], int32_t(mesh.vertex_offset), id);
            }
        }

        void trace(VkCommandBuffer cmd) {
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkz::barrier::push_and_flush(cmd, hdr_color.image, range, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                                         VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
            vkz::bind_pipeline(cmd, ray_pipeline);
            const std::array sets{scene_set, texture_set, ibl_set};
            vkCmdBindDescriptorSets(cmd, ray_pipeline.bind_point, ray_pipeline.layout, 0, ibl_layout ? 3 : 2, sets.data(), 0, nullptr);
            vkz::trace_ray(cmd, ray_pipeline, extent.width, extent.height);
        }

        vkz::image_view color_view() const {
            return raster_samples == VK_SAMPLE_COUNT_1_BIT ? hdr_color.image_view : multisample_color.image_view;
        }

        std::optional<vkz::image_view> resolve_view() const {
            if (raster_samples == VK_SAMPLE_COUNT_1_BIT) return {};
            return hdr_color.image_view;
        }

        void prepare_color(VkCommandBuffer cmd) {
            if (raster_samples != VK_SAMPLE_COUNT_1_BIT)
                vkz::barrier::push_and_flush(cmd, multisample_color.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                             VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            vkz::barrier::push_and_flush(cmd, hdr_color.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        void prepare_tone_mapping(VkCommandBuffer cmd) {
            vkz::barrier::push_and_flush(cmd, hdr_color.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                                         ray_traced ? VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                         ray_traced ? VK_ACCESS_2_SHADER_WRITE_BIT : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        void tone_map(VkCommandBuffer cmd, float exposure) {
            vkz::bind_pipeline(cmd, tone_mapping_pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tone_mapping_pipeline.layout, 0, 1, &tone_mapping_set, 0, nullptr);
            vkz::push_constants(cmd, tone_mapping_pipeline, tone_mapping_uniform{exposure}, VK_SHADER_STAGE_FRAGMENT_BIT);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        }

        void destroy() {
            tone_mapping_pipeline.destroy();
            if (tone_mapping_pool) vkDestroyDescriptorPool(device, tone_mapping_pool, nullptr);
            tone_mapping_layout.destroy();
            background_pipeline.destroy();
            if (ibl_pool) vkDestroyDescriptorPool(device, ibl_pool, nullptr);
            ibl_layout.destroy();
            ibl_buffer.destroy();
            environment.destroy();
            ray_pipeline.destroy();
            if (query_pool) vkDestroyDescriptorPool(device, query_pool, nullptr);
            query_layout.destroy();
            acceleration.reset();
            hdr_color.destroy();
            multisample_color.destroy();
            ray_instances.destroy();
            ray_materials.destroy();
            for (auto &pipeline : pipelines)
                pipeline.destroy();
            if (pool) vkDestroyDescriptorPool(device, pool, nullptr);
            for (auto &layout : layouts)
                layout.destroy();
            for (auto &buffer : material_buffers)
                buffer.destroy();
            scene_buffer.destroy();
            lights.destroy();
            for (auto &texture : textures)
                texture.destroy();
        }

      private:
        void init(vkz::vma_memory_allocator &allocator, VkQueue queue, uint32_t family, const vkz::mesh::data &scene, const vkz::buffer &vertices, uint32_t width, uint32_t height,
                  VkFormat color_format, VkFormat depth_format, const vkz::buffer &indices, const vkz::path &environment_path) {
            device = allocator.device;
            validate_scene(scene, color_format);
            ray_query = !ray_traced && std::any_of(scene.materials.begin(), scene.materials.end(), [](const auto &material) {
                return material.transmission_factor > 0;
            });
            create_hdr_target(allocator, width, height);
            multisample_color = make_multisample_color(allocator, {width, height}, hdr_format, raster_samples);
            create_tone_mapping(color_format);
            create_uniforms(allocator, scene);
            create_environment(allocator, queue, family, environment_path);
            create_fallback_texture(allocator, queue, family);
            load_textures(allocator, queue, family, scene);
            if (ray_traced) {
                create_ray_resources(allocator, scene);
                create_ray_layouts();
                create_ray_descriptor_pool();
                build_acceleration(allocator, queue, family, scene, vertices, indices);
                create_ray_descriptors(vertices, indices);
                ray_pipeline = vkz::reference::create_ray_tracing_pipeline({allocator, layouts, VKZ_COUNT(textures), ibl_layout});
                return;
            }
            create_descriptor_set_layouts();
            create_descriptor_pool(scene);
            create_scene_descriptors(vertices);
            create_material_descriptors(allocator, scene);
            if (ray_query) {
                create_ray_resources(allocator, scene);
                build_acceleration(allocator, queue, family, scene, vertices, indices);
                create_query_descriptors(vertices, indices);
            }
            create_pipelines(width, height, hdr_format, depth_format);
            if (ibl_layout) background_pipeline = vkz::reference::create_background_pipeline({device, ibl_layout, {width, height}, hdr_format, depth_format, raster_samples});
        }

        void create_environment(vkz::vma_memory_allocator &allocator, VkQueue queue, uint32_t family, const vkz::path &file) {
            if (file.empty()) return;
            environment = vkz::ibl::load(allocator, queue, family, file);
            ibl_buffer = make_buffer(allocator, sizeof(ibl_uniform));
            const auto stages = ray_traced ? VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR : VK_SHADER_STAGE_FRAGMENT_BIT;
            const auto bindings = ibl_bindings(stages);
            VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layout_info.bindingCount = VKZ_COUNT(bindings);
            layout_info.pBindings = bindings.data();
            ibl_layout.device = device;
            VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &ibl_layout.handle));
            const std::array<VkDescriptorPoolSize, 2> sizes{{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}}};
            VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool_info.maxSets = 1;
            pool_info.poolSizeCount = VKZ_COUNT(sizes);
            pool_info.pPoolSizes = sizes.data();
            VKZ_CHECK_VULKAN(vkCreateDescriptorPool(device, &pool_info, nullptr, &ibl_pool));
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocation.descriptorPool = ibl_pool;
            allocation.descriptorSetCount = 1;
            allocation.pSetLayouts = &ibl_layout.handle;
            VKZ_CHECK_VULKAN(vkAllocateDescriptorSets(device, &allocation, &ibl_set));
            bind_buffer(ibl_set, 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ibl_buffer);
            const std::array maps{&environment.environment, &environment.irradiance, &environment.specular, &environment.brdf_lut};
            for (uint32_t i = 0; i < maps.size(); ++i) {
                VkDescriptorImageInfo image_info{maps[i]->sampler, maps[i]->image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                VkWriteDescriptorSet descriptor{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                descriptor.dstSet = ibl_set;
                descriptor.dstBinding = i;
                descriptor.descriptorCount = 1;
                descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptor.pImageInfo = &image_info;
                vkUpdateDescriptorSets(device, 1, &descriptor, 0, nullptr);
            }
        }

        void validate_scene(const vkz::mesh::data &scene, VkFormat color_format) {
            if (scene.materials.empty()) throw std::invalid_argument("Reference renderer requires materials");
            static_cast<void>(vertex_stride(scene));
            if (color_format != VK_FORMAT_B8G8R8A8_SRGB && color_format != VK_FORMAT_R8G8B8A8_SRGB) throw std::runtime_error("Reference viewer requires an sRGB target for linear blending");
        }

        vkz::buffer make_buffer(vkz::vma_memory_allocator &allocator, size_t size) {
            return vkz::buffer::builder(allocator)
                    .usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                    .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                    .size(size)
                    .build();
        }

        void create_uniforms(vkz::vma_memory_allocator &allocator, const vkz::mesh::data &scene) {
            lights = upload_storage(allocator, make_light_uniforms(scene));
            scene_buffer = make_buffer(allocator, (ray_traced || ray_query) ? sizeof(ray_tracing_scene_uniform) : sizeof(scene_uniform));
        }

        void create_fallback_texture(vkz::vma_memory_allocator &allocator, VkQueue queue, uint32_t family) {
            textures.resize(1);
            auto &white = textures.front();
            white.image = vkz::image::builder(allocator)
                    .format(VK_FORMAT_R8G8B8A8_UNORM)
                    .extent(1, 1)
                    .usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                    .build();
            white.image_view = vkz::image_view::builder(device)
                    .image(white.image)
                    .format(VK_FORMAT_R8G8B8A8_UNORM)
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .level_count(1)
                    .layer_count(1)
                    .build();
            white.sampler = vkz::sampler::builder(device)
                    .build();
            {
                vkz::command_pool uploads{device, family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue};
                auto cmd = uploads.create_command_buffer();
                vkz::clear(cmd, white.image, VkClearColorValue{{1, 1, 1, 1}}, {});
                vkz::barrier::push_and_flush(cmd, white.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                             VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                uploads.submit_and_wait(cmd);
            }
        }

        void load_textures(vkz::vma_memory_allocator &allocator, VkQueue queue, uint32_t family, const vkz::mesh::data &scene) {
            for (const auto &texture : scene.textures) {
                const auto settings = texture.sampler_id < 0 ? vkz::mesh::sampler{} : scene.samplers.at(texture.sampler_id);
                textures.push_back(vkz::texture_from_ktx(scene.images.at(texture.image_id).encoded_data, allocator, queue, family, settings));
            }
        }

        void create_descriptor_set_layouts() {
            VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layout_info.bindingCount = uint32_t(scene_bindings.size());
            layout_info.pBindings = scene_bindings.data();
            layouts[0].device = device;
            layouts[1].device = device;
            VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &layouts[0].handle));
            layout_info.bindingCount = uint32_t(material_bindings.size());
            layout_info.pBindings = material_bindings.data();
            VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &layouts[1].handle));
        }

        void create_descriptor_pool(const vkz::mesh::data &scene) {
            const auto count = uint32_t(scene.materials.size());
            const std::array<VkDescriptorPoolSize, 3> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count + 1},
                                                             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
                                                             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count * uint32_t(vkz::mesh::material_texture_count)}}};
            VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool_info.maxSets = count + 1;
            pool_info.poolSizeCount = uint32_t(sizes.size());
            pool_info.pPoolSizes = sizes.data();
            VKZ_CHECK_VULKAN(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));
        }

        VkDescriptorSet allocate(VkDescriptorSetLayout layout) {
            VkDescriptorSet set{};
            VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            info.descriptorPool = pool;
            info.descriptorSetCount = 1;
            info.pSetLayouts = &layout;
            VKZ_CHECK_VULKAN(vkAllocateDescriptorSets(device, &info, &set));
            return set;
        }

        void bind_buffer(VkDescriptorSet set, uint32_t binding, VkDescriptorType type, const vkz::buffer &buffer) {
            VkDescriptorBufferInfo info{buffer, 0, VK_WHOLE_SIZE};
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = set;
            write.dstBinding = binding;
            write.descriptorCount = 1;
            write.descriptorType = type;
            write.pBufferInfo = &info;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }

        void create_scene_descriptors(const vkz::buffer &vertices) {
            scene_set = allocate(layouts[0].handle);
            bind_buffer(scene_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, scene_buffer);
            bind_buffer(scene_set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertices);
            bind_buffer(scene_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lights);
        }

        void create_material_descriptors(vkz::vma_memory_allocator &allocator, const vkz::mesh::data &scene) {
            for (const auto &mat : scene.materials) {
                const auto data = make_material_uniform(scene, mat);
                const auto slots = vkz::mesh::texture_slots(mat);
                const auto set = allocate(layouts[1].handle);
                for (uint32_t i = 0; i < slots.size(); ++i) {
                    const auto &slot = *slots[i];
                    const auto &tex = textures.at(slot.index + 1);
                    VkDescriptorImageInfo info{tex.sampler, tex.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                    write.dstSet = set;
                    write.dstBinding = i + 1;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    write.descriptorCount = 1;
                    write.pImageInfo = &info;
                    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
                }
                material_buffers.push_back(make_buffer(allocator, sizeof(data)));
                write(material_buffers.back(), data);
                bind_buffer(set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, material_buffers.back());
                material_sets.push_back(set);
            }
        }

        void create_pipelines(uint32_t width, uint32_t height, VkFormat color_format, VkFormat depth_format) {
            for (uint32_t i = 0; i < pipelines.size(); ++i) {
                pipelines[i] = vkz::reference::create_pipeline({.device = device,
                                                                .layouts = layouts,
                                                                .extent = {width, height},
                                                                .color_format = color_format,
                                                                .depth_format = depth_format,
                                                                .samples = raster_samples,
                                                                .alpha_blend = bool(i & 2),
                                                                .mirrored = bool(i & 1),
                                                                .ibl_layout = ibl_layout,
                                                                .ray_query_layout = query_layout,
                                                                .texture_count = VKZ_COUNT(textures)});
            }
        }

        template <class T> vkz::buffer upload_storage(vkz::vma_memory_allocator &allocator, const std::vector<T> &data) {
            auto result = vkz::buffer::builder(allocator)
                    .usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                    .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                    .size(VKZ_BYTE_SIZE(data))
                    .build();
            auto mapping = result.map();
            std::memcpy(mapping.as<void>(), data.data(), VKZ_BYTE_SIZE(data));
            VKZ_CHECK_VULKAN(vmaFlushAllocation(result.allocator, result.allocation, 0, VK_WHOLE_SIZE));
            mapping.unmap();
            return result;
        }

        void create_hdr_target(vkz::vma_memory_allocator &allocator, uint32_t width, uint32_t height) {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(device.physical, hdr_format, &properties);
            const auto required =
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | (ray_traced ? VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT : 0);
            VKZ_ASSERT((properties.optimalTilingFeatures & required) == required, "Reference viewer requires a blendable, sampled HDR target");
            extent = {width, height};
            hdr_color.image = vkz::image::builder(allocator)
                .format(hdr_format)
                .extent(width, height)
                .usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | (ray_traced ? VK_IMAGE_USAGE_STORAGE_BIT : 0))
                .build();
            hdr_color.image_view = vkz::image_view::builder(device)
                .image(hdr_color.image)
                .format(hdr_format)
                .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .level_count(1)
                .layer_count(1)
                .build();
            hdr_color.sampler = vkz::sampler::builder(device)
                .min_filter(VK_FILTER_NEAREST)
                .mag_filter(VK_FILTER_NEAREST)
                .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
                .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                .max_lod(0)
                .build();
        }

        void create_tone_mapping(VkFormat color_format) {
            VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layout_info.bindingCount = 1;
            layout_info.pBindings = &tone_mapping_binding;
            tone_mapping_layout.device = device;
            VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &tone_mapping_layout.handle));
            VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
            VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool_info.maxSets = 1;
            pool_info.poolSizeCount = 1;
            pool_info.pPoolSizes = &size;
            VKZ_CHECK_VULKAN(vkCreateDescriptorPool(device, &pool_info, nullptr, &tone_mapping_pool));
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocation.descriptorPool = tone_mapping_pool;
            allocation.descriptorSetCount = 1;
            allocation.pSetLayouts = &tone_mapping_layout.handle;
            VKZ_CHECK_VULKAN(vkAllocateDescriptorSets(device, &allocation, &tone_mapping_set));
            VkDescriptorImageInfo image_info{hdr_color.sampler, hdr_color.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet descriptor{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descriptor.dstSet = tone_mapping_set;
            descriptor.descriptorCount = 1;
            descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor.pImageInfo = &image_info;
            vkUpdateDescriptorSets(device, 1, &descriptor, 0, nullptr);
            tone_mapping_pipeline = create_tone_mapping_pipeline({device, tone_mapping_layout, extent, color_format});
        }

        void create_ray_resources(vkz::vma_memory_allocator &allocator, const vkz::mesh::data &scene) {
            ray_instances = upload_storage(allocator, make_ray_tracing_instances(scene));
            std::vector<material_uniform> materials;
            for (const auto &material : scene.materials)
                materials.push_back(make_material_uniform(scene, material));
            ray_materials = upload_storage(allocator, materials);
        }

        void create_ray_layouts() {
            const auto texture_binding = ray_tracing_texture_binding(VKZ_COUNT(textures));
            VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            for (size_t i = 0; i < layouts.size(); ++i) {
                layouts[i].device = device;
                info.bindingCount = i == 0 ? VKZ_COUNT(ray_tracing_scene_bindings) : 1;
                info.pBindings = i == 0 ? ray_tracing_scene_bindings.data() : &texture_binding;
                VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device, &info, nullptr, &layouts[i].handle));
            }
        }

        void create_ray_descriptor_pool() {
            const std::array<VkDescriptorPoolSize, 5> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                                             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5},
                                                             {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
                                                             {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                                                             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VKZ_COUNT(textures)}}};
            VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            info.maxSets = 2;
            info.poolSizeCount = VKZ_COUNT(sizes);
            info.pPoolSizes = sizes.data();
            VKZ_CHECK_VULKAN(vkCreateDescriptorPool(device, &info, nullptr, &pool));
        }

        void build_acceleration(vkz::vma_memory_allocator &allocator, VkQueue queue, uint32_t family, const vkz::mesh::data &scene, const vkz::buffer &vertices, const vkz::buffer &indices) {
            acceleration = std::make_unique<vkz::mesh::bvh>(allocator);
            vkz::command_pool commands{device, family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue};
            auto cmd = commands.create_command_buffer();
            vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
            acceleration->build(cmd, scene, {vertices, indices});
            if (ray_query) vkz::barrier::acceleration_structure_update_to_ray_query_read(cmd);
            commands.submit_and_wait(cmd);
        }

        void create_ray_descriptors(const vkz::buffer &vertices, const vkz::buffer &indices) {
            scene_set = allocate(layouts[0].handle);
            texture_set = allocate(layouts[1].handle);
            bind_buffer(scene_set, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lights);
            const std::array buffers{scene_buffer, vertices, indices, ray_instances, ray_materials};
            for (uint32_t i = 0; i < buffers.size(); ++i)
                bind_buffer(scene_set, i, i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffers[i]);
            const auto handle = acceleration->tlas().handle;
            VkWriteDescriptorSetAccelerationStructureKHR acceleration_info{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
            acceleration_info.accelerationStructureCount = 1;
            acceleration_info.pAccelerationStructures = &handle;
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.pNext = &acceleration_info;
            write.dstSet = scene_set;
            write.dstBinding = 5;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
            VkDescriptorImageInfo output_info{{}, hdr_color.image_view, VK_IMAGE_LAYOUT_GENERAL};
            write.pNext = nullptr;
            write.dstBinding = 6;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write.pImageInfo = &output_info;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
            std::vector<VkDescriptorImageInfo> images;
            for (const auto &texture : textures)
                images.push_back({texture.sampler, texture.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            write.dstSet = texture_set;
            write.dstBinding = 0;
            write.descriptorCount = VKZ_COUNT(images);
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = images.data();
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }

        void create_query_descriptors(const vkz::buffer &vertices, const vkz::buffer &indices) {
            const auto bindings = ray_query_bindings(VKZ_COUNT(textures));
            VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layout_info.bindingCount = VKZ_COUNT(bindings);
            layout_info.pBindings = bindings.data();
            query_layout.device = device;
            VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &query_layout.handle));
            const std::array<VkDescriptorPoolSize, 4> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                                             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5},
                                                             {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
                                                             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VKZ_COUNT(textures)}}};
            VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool_info.maxSets = 1;
            pool_info.poolSizeCount = VKZ_COUNT(sizes);
            pool_info.pPoolSizes = sizes.data();
            VKZ_CHECK_VULKAN(vkCreateDescriptorPool(device, &pool_info, nullptr, &query_pool));
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocation.descriptorPool = query_pool;
            allocation.descriptorSetCount = 1;
            allocation.pSetLayouts = &query_layout.handle;
            VKZ_CHECK_VULKAN(vkAllocateDescriptorSets(device, &allocation, &query_set));
            const std::array buffers{scene_buffer, vertices, indices, ray_instances, ray_materials};
            for (uint32_t i = 0; i < buffers.size(); ++i)
                bind_buffer(query_set, i, i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffers[i]);
            const auto handle = acceleration->tlas().handle;
            VkWriteDescriptorSetAccelerationStructureKHR acceleration_info{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
            acceleration_info.accelerationStructureCount = 1;
            acceleration_info.pAccelerationStructures = &handle;
            VkWriteDescriptorSet descriptor{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descriptor.pNext = &acceleration_info;
            descriptor.dstSet = query_set;
            descriptor.dstBinding = 5;
            descriptor.descriptorCount = 1;
            descriptor.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            vkUpdateDescriptorSets(device, 1, &descriptor, 0, nullptr);
            std::vector<VkDescriptorImageInfo> images;
            for (const auto &texture : textures)
                images.push_back({texture.sampler, texture.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            descriptor.pNext = nullptr;
            descriptor.dstBinding = 6;
            descriptor.descriptorCount = VKZ_COUNT(images);
            descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor.pImageInfo = images.data();
            vkUpdateDescriptorSets(device, 1, &descriptor, 0, nullptr);
        }

        bool ray_query{};
        vkz::descriptor_set_layout query_layout;
        VkDescriptorPool query_pool{};
        VkDescriptorSet query_set{};
        static constexpr VkFormat hdr_format = VK_FORMAT_R32G32B32A32_SFLOAT;
        vkz::descriptor_set_layout tone_mapping_layout;
        VkDescriptorPool tone_mapping_pool{};
        VkDescriptorSet tone_mapping_set{};
        vkz::pipeline tone_mapping_pipeline;
        vkz::ibl::maps environment;
        vkz::descriptor_set_layout ibl_layout;
        VkDescriptorPool ibl_pool{};
        VkDescriptorSet ibl_set{};
        vkz::buffer ibl_buffer;
        vkz::pipeline background_pipeline;
        bool ray_traced{};
        uint32_t samples{8};
        VkExtent2D extent{};
        vkz::ray_tracing_pipeline ray_pipeline;
        std::unique_ptr<vkz::mesh::bvh> acceleration;
        vkz::texture hdr_color, multisample_color;
        VkSampleCountFlagBits raster_samples{VK_SAMPLE_COUNT_1_BIT};
        vkz::buffer ray_instances, ray_materials;
        VkDescriptorSet texture_set{};
        vkz::device device;
        VkDescriptorPool pool{};
        std::array<vkz::descriptor_set_layout, 2> layouts{};
        VkDescriptorSet scene_set{};
        std::vector<VkDescriptorSet> material_sets;
        vkz::buffer scene_buffer;
        vkz::buffer lights;
        std::vector<vkz::buffer> material_buffers;
        std::vector<vkz::texture> textures;
        std::array<vkz::pipeline, 4> pipelines{};
    };
}
