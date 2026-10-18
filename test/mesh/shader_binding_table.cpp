#define VKZ_IOSTREAM_ADAPTER
#include <vulkanizer/application.hpp>
#include <vulkanizer/bvh.hpp>
#include <vulkanizer/ray_tracing_pipeline_builder.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmrc/cmrc.hpp>
#include <imgui_internal.h>
#include "../timeout.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string_view>

CMRC_DECLARE(sbt_shaders);

namespace {
    constexpr uint32_t width = 960;
    constexpr uint32_t height = 640;
    constexpr vkz::shader_stage ray_stages = vkz::shader_stage_raygen | vkz::shader_stage_closest_hit;
    const std::array<glm::vec4, 8> colors{{{1, 0, 0, 1}, {1, 0.5f, 0, 1}, {0, 1, 0, 1}, {0, 1, 1, 1}, {0, 0, 1, 1}, {1, 0, 1, 1}, {1, 1, 0, 1}, {1, 1, 1, 1}}};
    const std::array<glm::vec4, 2> backgrounds{{{0.025f, 0.035f, 0.055f, 1}, {0.1f, 0.08f, 0.12f, 1}}};

    std::vector<uint32_t> shader(const char *name) {
        const auto file = cmrc::sbt_shaders::get_filesystem().open(name);
        VKZ_ASSERT(file.size() % sizeof(uint32_t) == 0, "Invalid embedded SBT shader");
        std::vector<uint32_t> words(file.size() / sizeof(uint32_t));
        std::memcpy(words.data(), file.begin(), file.size());
        return words;
    }

    vkz::vulkan_app_create_info create_info() {
        VkPhysicalDeviceBufferDeviceAddressFeatures address{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .bufferDeviceAddress = VK_TRUE
        };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .accelerationStructure = VK_TRUE
        };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR tracing{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .rayTracingPipeline = VK_TRUE
        };

        vkz::vulkan_app_create_info result{
            .width = 1280,
            .height = 800,
            .title = "Shader binding table - Stanford Bunny",
            .resizable = false,
            .validation = true,
            .device_extensions = {
                VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
            },
        };
        result.extension_chain.add(address).add(acceleration).add(tracing);
        return result;
    }

    class sbt_demo : public vkz::application {
      public:
        explicit sbt_demo(const test_timeout &timeout, bool hidden)
            : application(create_info())
            , timeout(timeout)
            , ray_allocator(vkz::vma_memory_allocator::create(context_, VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT))
            , acceleration(ray_allocator)
            , input(app_.window()) {
            if (hidden) glfwHideWindow(app_.window());
        }

        ~sbt_demo() {
            vkDeviceWaitIdle(device_);
            pipeline.destroy();
            acceleration.destroy();
            if (display_set) vkz::imgui::remove_texture(display_set);
            descriptors.destroy();
            layout.destroy();
            display.destroy();
            vertices.destroy();
            indices.destroy();
            camera_buffer.destroy();
            readback.destroy();
            ray_allocator.destroy();
        }

        void setup() override {
            load_scene();
            create_geometry_buffers();
            create_display();
            create_camera();
            create_descriptors();
            create_pipeline();
            build_acceleration();
            update_descriptors();
        }

        void new_frame() override {
            if (timeout.expired()) glfwSetWindowShouldClose(app_.window(), GLFW_TRUE);
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({1280, 800});
            ImGui::Begin("Shader binding table", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            ImGui::BeginChild("Controls", {290, 0});
            ImGui::TextUnformatted("Stanford Bunny / SBT records");
            ImGui::SliderInt("Ray offset", &ray_offset, 0, 1);
            ImGui::SliderInt("Ray stride", &ray_stride, 0, 15);
            ImGui::SliderInt("Miss record", &miss_record, 0, 1);
            ImGui::SliderInt("Cull mask", &cull_mask, 0, 7);
            ImGui::Checkbox("Flat record colours", &flat_color);
            ImGui::Checkbox("Alternate ray generation", &alternate_ray_generation);
            for (int i = 0; i < 3; ++i) {
                ImGui::PushID(i);
                ImGui::Text("Instance %d, mask %d", i, 1 << i);
                if (ImGui::SliderInt("Instance offset", &offsets[i], 0, 6)) instances_dirty = true;
                ImGui::Text("Selected hit record: %d", offsets[i] + ray_offset);
                ImGui::PopID();
            }
            ImGui::Separator();
            ImGui::TextWrapped("Record = instance offset + geometry index * ray stride + ray offset. Our mesh BLAS has one geometry, so geometry index is zero.");
            for (int i = 0; i < static_cast<int>(colors.size()); ++i) {
                ImGui::PushID(100 + i);
                ImGui::ColorButton("record", {colors[i].x, colors[i].y, colors[i].z, 1});
                ImGui::SameLine();
                ImGui::Text("Hit record %d", i);
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(display_set)), {960, 640});
            ImGui::ItemHoverable({ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}, ImGui::GetID("scene_view"), ImGuiItemFlags_None);
            ImGui::End();
            process_camera_input();
        }

        void app_logic(VkCommandBuffer command_buffer) override {
            update_camera();
            update_instances(command_buffer);
            trace(command_buffer);
        }

        void verify() {
            setup();
            verify_camera_input();
            flat_color = true;
            verify_case({0, 2, 4}, 1.0f, "initial hit records");
            ray_offset = 1;
            verify_case({1, 3, 5}, 1.0f, "ray offset");
            ray_stride = 3;
            verify_case({1, 3, 5}, 1.0f, "single-geometry ray stride");
            offsets[0] = 6;
            instances_dirty = true;
            verify_case({7, 3, 5}, 1.0f, "TLAS instance offset update");
            cull_mask = 1;
            verify_case({7}, 1.0f, "instance masks");
            miss_record = 1;
            verify_case({7}, 1.0f, "miss records");
            alternate_ray_generation = true;
            verify_case({7}, 0.5f, "named ray generation and specialization");
            vkz::ray_tracing_pipeline moved = std::move(pipeline);
            VKZ_ASSERT(!pipeline.handle && moved.handle, "Pipeline move must transfer ownership");
            pipeline = std::move(moved);
            verify_case({7}, 0.5f, "pipeline ownership transfer");
            std::cout << "Shader binding table GPU verification passed\n";
        }

      private:
        struct parameters {
            uint32_t cull_mask{0xff};
            uint32_t ray_offset{};
            uint32_t ray_stride{};
            uint32_t miss_index{};
            uint32_t vertex_stride{};
            uint32_t normal_offset{};
            uint32_t vertex_offset{};
            uint32_t index_offset{};
            uint32_t flat_color{};
        } constants;

        void load_scene() {
            const auto files = vkz::mesh::convert(VKZ_BUNNY_SOURCE, VKZ_BUNNY_CONVERTED);
            vkz::mesh::load_mesh_data(files.mesh_file, scene);
            vkz::mesh::load_mesh_data_materials(files.material_file, scene);
            vkz::mesh::load_bounding_boxes(files.bounds_file, scene.boxes);
            VKZ_ASSERT(scene.meshes.size() == 1, "Bunny demo expects one mesh");

            const auto bounds = scene.boxes.front();
            const auto size = bounds.get_size();
            const float scale = 1.8f / std::max({size.x, size.y, size.z});
            const auto normalize = glm::scale(glm::mat4{1}, glm::vec3{scale}) * glm::translate(glm::mat4{1}, -bounds.get_center());

            scene.instances.clear();
            for (int i = 0; i < 3; ++i) {
                const auto model = glm::translate(glm::mat4{1}, glm::vec3{(static_cast<float>(i - 1)) * 1.8f, 0, 0}) * normalize;
                scene.instances.push_back({model, glm::inverse(model), 0});
            }

            constants.vertex_stride = scene.streams.bindings.front().stride / sizeof(float);
            const auto normal = std::find_if(scene.streams.attributes.begin(), scene.streams.attributes.end(), [](const auto &value) {
                return value.location == 1;
            });

            VKZ_ASSERT(normal != scene.streams.attributes.end(), "Bunny requires normals");
            constants.normal_offset = normal->offset / sizeof(float);
            constants.vertex_offset = scene.meshes.front().vertex_offset;
            constants.index_offset = scene.meshes.front().index_offset + scene.meshes.front().lod_offset[0];
        }

        vkz::buffer upload(const void *data, VkDeviceSize size, vkz::buffer_usage usage) {
            auto result = vkz::buffer::builder(ray_allocator)
                    .size(size)
                    .usage(usage)
                    .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                    .build();
            auto mapping = result.map();
            std::memcpy(mapping.as<void>(), data, static_cast<size_t>(size));
            const auto status = vmaFlushAllocation(result.allocator, result.allocation, 0, VK_WHOLE_SIZE);
            mapping.unmap();
            VKZ_CHECK_VULKAN(status);
            return result;
        }

        void create_geometry_buffers() {
            constexpr auto usage = vkz::buffer_usage_acceleration_storage_input;
            vertices = upload(scene.vertex_data.data(), VKZ_BYTE_SIZE(scene.vertex_data), usage);
            indices = upload(scene.index_data.data(), VKZ_BYTE_SIZE(scene.index_data), usage);
        }

        void create_display() {
            display.image = vkz::image::builder(ray_allocator)
                .format(VK_FORMAT_R32G32B32A32_SFLOAT)
                .extent(width, height)
                .usage(vkz::image_usage_sampled_storage_download)
                .build();
            display.image_view =
                vkz::image_view::builder(device_)
                    .image(display.image)
                    .format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .aspect_mask(vkz::image_aspect_color)
                    .level_count(1)
                    .layer_count(1)
                    .build();
            display.sampler = vkz::sampler::builder(device_)
                    .build();
            display_set = vkz::imgui::add_texture(display.sampler, display.image_view);
            readback = vkz::buffer::builder(ray_allocator)
                .size(width * height * sizeof(glm::vec4))
                .usage(vkz::buffer_usage_transfer_dst)
                .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
                .build();
        }

        void create_camera() {
            input.bind();
            const auto elevation = glm::radians(8.0f);
            camera.position = {0, 6.5f * std::sin(elevation), 6.5f * std::cos(elevation)};
            camera.minZoom = 3;
            camera.maxZoom = 12;
            vkz::camera::orbit initializer{camera};
            initializer.look_at(camera.position, {0, 0, 0}, {0, 1, 0});
            initializer.perspective(45, float(width) / height, 0.01f, 100);
            controller = std::make_unique<vkz::camera::controller>(camera, vkz::camera::movement_type::orbit, input.get_device());
            const std::array<glm::mat4, 2> data{glm::mat4{1}, glm::mat4{1}};
            camera_buffer = upload(data.data(), sizeof(data), vkz::buffer_usage_uniform);
            update_camera();
        }

        void process_camera_input() {
            if (ImGui::IsAnyItemActive()) {
                auto &device = input.get_device();
                device.mouse.relative_position = {};
                device.mouse.scroll_offset = {};
                for (auto &[mapping, button] : device.mappings)
                    button.pressed = false;
                return;
            }
            controller->process_input();
            controller->update(ImGui::GetIO().DeltaTime);
        }

        void update_camera() {
            const std::array<glm::mat4, 2> data{glm::inverse(camera.view), glm::inverse(camera.projection)};
            auto mapping = camera_buffer.map();
            std::memcpy(mapping.as<void>(), data.data(), sizeof(data));
            const auto status = vmaFlushAllocation(camera_buffer.allocator, camera_buffer.allocation, 0, VK_WHOLE_SIZE);
            mapping.unmap();
            VKZ_CHECK_VULKAN(status);
        }

        void create_descriptors() {
            layout = vkz::descriptor_set_layout_builder{device_}
                         .binding(0)
                         .descriptor_type(vkz::descriptor_type_acceleration_structure)
                         .descriptor_count(1)
                         .shader_stages(vkz::shader_stage_raygen)
                         .binding(1)
                         .descriptor_type(vkz::descriptor_type_storage_image)
                         .descriptor_count(1)
                         .shader_stages(vkz::shader_stage_raygen)
                         .binding(2)
                         .descriptor_type(vkz::descriptor_type_uniform_buffer)
                         .descriptor_count(1)
                         .shader_stages(vkz::shader_stage_raygen)
                         .binding(3)
                         .descriptor_type(vkz::descriptor_type_storage_buffer)
                         .descriptor_count(1)
                         .shader_stages(vkz::shader_stage_closest_hit)
                         .binding(4)
                         .descriptor_type(vkz::descriptor_type_storage_buffer)
                         .descriptor_count(1)
                         .shader_stages(vkz::shader_stage_closest_hit)
                         .create_layout();
            descriptors = vkz::descriptor_pool{device_,
                                               1,
                                               {{static_cast<VkDescriptorType>(vkz::descriptor_type_acceleration_structure), 1},
                                                {static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), 1},
                                                {static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 1},
                                                {static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 2}}};
            descriptor_set = descriptors.allocate(layout);
        }

        void create_pipeline() {
            vkz::ray_tracing_pipeline_builder builder{ray_allocator};
            builder
                .shader_groups()
                    .ray_generation_group("primary")
                        .ray_generation_shader(shader("sbt.rgen.spv"))
                    .miss_group("background")
                        .miss_shader(shader("sbt.rmiss.spv"))
                        .record_data(backgrounds[0]);
            for (uint32_t i = 0; i < colors.size(); ++i) {
                builder
                    .shader_groups()
                        .triangles_hit_group("hit_" + std::to_string(i))
                            .closest_hit_shader(shader("sbt.rchit.spv"))
                            .record_data(colors[i]);
            }
            pipeline = builder
                    .shader_groups()
                        .ray_generation_group("dimmer")
                            .ray_generation_shader(shader("sbt.rgen.spv"))
                            .add_specialization(0.5f, 0)
                        .miss_group("alternate_background")
                            .miss_shader(shader("sbt.rmiss.spv"))
                            .record_data(backgrounds[1])
                    .layout()
                        .add_descriptor_set_layout(layout)
                        .add_push_constant_range(ray_stages, 0, sizeof(parameters))
                    .max_recursion_depth(1)
                    .name("shader_binding_table_demo")
                    .build();
        }

        void build_acceleration() {
            for (size_t i = 0; i < instance_options.size(); ++i)
                instance_options[i] = {static_cast<uint8_t>(1u << i), static_cast<uint32_t>(offsets[i])};
            vkz::command_pool commands{device_, app_.queue_family_index(), vkz::command_pool_flags_transient, app_.graphics_queue()};
            auto command_buffer = commands.create_command_buffer();
            acceleration.build(command_buffer, scene, {vertices, indices}, {}, instance_options);
            commands.submit_and_wait(command_buffer);
        }

        void update_descriptors() {
            const auto handle = acceleration.tlas().handle;
            VkWriteDescriptorSetAccelerationStructureKHR acceleration_info{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
            acceleration_info.accelerationStructureCount = 1;
            acceleration_info.pAccelerationStructures = &handle;
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.pNext = &acceleration_info;
            write.dstSet = descriptor_set.handle;
            write.dstBinding = 0;
            write.descriptorType = static_cast<VkDescriptorType>(vkz::descriptor_type_acceleration_structure);
            write.descriptorCount = 1;
            vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
            vkz::update_descriptor(device_, {descriptor_set,
                                             {vkz::image_descriptor{display.image_view, VK_IMAGE_LAYOUT_GENERAL, 1}, vkz::ubo_descriptor{camera_buffer, 0, VK_WHOLE_SIZE, 2},
                                              vkz::buffer_descriptor{vertices, 0, VK_WHOLE_SIZE, 3}, vkz::buffer_descriptor{indices, 0, VK_WHOLE_SIZE, 4}}});
        }

        void update_instances(VkCommandBuffer command_buffer) {
            if (!instances_dirty) return;
            for (size_t i = 0; i < offsets.size(); ++i)
                instance_options[i].shader_binding_table_offset = static_cast<uint32_t>(offsets[i]);
            acceleration.update_instances(command_buffer, scene, instance_options);
            instances_dirty = false;
        }

        void trace(VkCommandBuffer command_buffer) {
            const VkImageSubresourceRange range{static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1};
            vkz::barrier::push_and_flush(command_buffer, display.image, range, vkz::pipeline_stage_all_commands, vkz::pipeline_stage_ray_tracing_shader, vkz::access_memory_read_write,
                                         vkz::access_shader_write, VK_IMAGE_LAYOUT_GENERAL);
            constants.cull_mask = static_cast<uint32_t>(cull_mask);
            constants.ray_offset = static_cast<uint32_t>(ray_offset);
            constants.ray_stride = static_cast<uint32_t>(ray_stride);
            constants.miss_index = static_cast<uint32_t>(miss_record);
            constants.flat_color = flat_color;
            vkz::bind_pipeline(command_buffer, pipeline);
            vkCmdBindDescriptorSets(command_buffer, pipeline.bind_point, pipeline.layout, 0, 1, &descriptor_set.handle, 0, nullptr);
            vkCmdPushConstants(command_buffer, pipeline.layout, static_cast<VkShaderStageFlags>(ray_stages), 0, sizeof(constants), &constants);
            vkz::trace_ray(command_buffer, pipeline, width, height, alternate_ray_generation ? "dimmer" : "primary");
            vkz::barrier::push_and_flush(command_buffer, display.image, range, vkz::pipeline_stage_ray_tracing_shader, vkz::pipeline_stage_fragment_shader, vkz::access_shader_write,
                                         vkz::access_shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        void verify_camera_input() {
            const auto initial_camera = camera;
            auto &io = ImGui::GetIO();
            auto &device = input.get_device();
            vkz::imgui::new_frame();
            new_frame();
            ImGui::EndFrame();
            io.AddMousePosEvent(700, 300);
            ImGui::NewFrame();
            new_frame();
            ImGui::EndFrame();
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
            ImGui::NewFrame();
            new_frame();
            ImGui::EndFrame();
            ImGui::NewFrame();
            device.mouse.relative_position = {30, 10};
            new_frame();
            VKZ_ASSERT(camera.view != initial_camera.view, "Dragging the scene must orbit the camera");
            ImGui::EndFrame();
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
            ImGui::NewFrame();
            new_frame();
            ImGui::Begin("Camera input verification");
            ImGui::SetActiveID(ImGui::GetID("active_control"), ImGui::GetCurrentWindow());
            const auto blocked_view = camera.view;
            device.mouse.relative_position = {30, 10};
            device.mouse.scroll_offset = {0, 1};
            process_camera_input();
            VKZ_ASSERT(camera.view == blocked_view, "Active ImGui items must block camera input");
            VKZ_ASSERT(device.mouse.relative_position == glm::vec2{} && device.mouse.scroll_offset == glm::vec2{}, "Blocked camera input must be discarded");
            ImGui::ClearActiveID();
            ImGui::End();
            ImGui::EndFrame();
            camera = initial_camera;
            std::cout << "Passed: scene drag and active-item camera guard\n";
        }

        void verify_case(std::initializer_list<int> expected, float exposure, const char *name) {
            vkz::command_pool commands{device_, app_.queue_family_index(), vkz::command_pool_flags_transient, app_.graphics_queue()};
            auto command_buffer = commands.create_command_buffer();
            app_logic(command_buffer);
            const VkImageSubresourceRange range{static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1};
            vkz::barrier::push_and_flush(command_buffer, display.image, range, vkz::pipeline_stage_all_commands, vkz::pipeline_stage_transfer, vkz::access_memory_read_write,
                                         vkz::access_transfer_read, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 0, 1};
            copy.imageExtent = {width, height, 1};
            vkCmdCopyImageToBuffer(command_buffer, display.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
            vkz::barrier::push_and_flush(command_buffer, vkz::pipeline_stage_transfer, vkz::pipeline_stage_host, vkz::access_transfer_write, vkz::access_host_read);
            commands.submit_and_wait(command_buffer);
            VKZ_CHECK_VULKAN(vmaInvalidateAllocation(readback.allocator, readback.allocation, 0, VK_WHOLE_SIZE));
            auto mapping = readback.map();
            const auto *pixels = mapping.as<glm::vec4>();
            std::array<size_t, 8> counts{};
            size_t background_count{};
            size_t unexpected{};
            for (size_t i = 0; i < width * height; ++i) {
                const glm::vec3 pixel = glm::vec3(pixels[i]);
                bool recognized{};
                for (size_t c = 0; c < colors.size(); ++c) {
                    if (glm::length(pixel - glm::vec3(colors[c]) * exposure) < 0.001f) {
                        ++counts[c];
                        recognized = true;
                    }
                }
                if (glm::length(pixel - glm::vec3(backgrounds[miss_record]) * exposure) < 0.001f) {
                    ++background_count;
                    recognized = true;
                }
                if (!recognized) ++unexpected;
            }
            mapping.unmap();
            if (unexpected || !background_count) VKZ_THROW(std::string("Unexpected pixels in ") + name);
            for (size_t i = 0; i < counts.size(); ++i) {
                const bool wanted = std::find(expected.begin(), expected.end(), static_cast<int>(i)) != expected.end();
                if ((wanted && counts[i] < 100) || (!wanted && counts[i] != 0)) VKZ_THROW(std::string("Wrong SBT hit record in ") + name);
            }
            std::cout << "Passed: " << name << '\n';
        }

        const test_timeout &timeout;
        vkz::vma_memory_allocator ray_allocator;
        vkz::mesh::data scene;
        vkz::bvh acceleration;
        vkz::buffer vertices;
        vkz::buffer indices;
        vkz::buffer camera_buffer;
        vkz::buffer readback;
        vkz::texture display;
        VkDescriptorSet display_set{};
        vkz::descriptor_set_layout layout{};
        vkz::descriptor_pool descriptors;
        vkz::descriptor_set descriptor_set{};
        vkz::ray_tracing_pipeline pipeline;
        std::array<vkz::bvh_instance_options, 3> instance_options{};
        std::array<int, 3> offsets{0, 2, 4};
        int cull_mask{7};
        int ray_offset{};
        int ray_stride{};
        int miss_record{};
        bool flat_color{};
        bool alternate_ray_generation{};
        bool instances_dirty{};
        vkz::camera::camera camera;
        vkz::glfw_input_adaptor input;
        std::unique_ptr<vkz::camera::controller> controller;
    };
}

int main(int argc, char **argv) try {
    vkz::iostream_adapter::install(std::cout);
    const test_timeout timeout{argc, argv};
    bool verify{};
    for (int i = 1; i < argc; ++i)
        if (std::string_view(argv[i]) == "--verify") verify = true;
    sbt_demo demo{timeout, verify};
    if (verify)
        demo.verify();
    else
        demo.main_loop();
    return 0;
} catch (const std::exception &error) {

    std::cerr << error.what() << '\n';
    return 1;
}
