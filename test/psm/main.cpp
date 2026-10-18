#define VKZ_IOSTREAM_ADAPTER

#include <vulkanizer/psm.hpp>
#include <vulkanizer/application.hpp>
#include <vulkanizer/mesh.hpp>
#include <vulkanizer/builders.hpp>
#include <imgui.h>
#include <stb.h>
#include <cmath>

namespace {

    struct render_constants {
        glm::mat4 model;
        glm::vec4 base_color;
    };

    struct scene_constants {
        glm::mat4 view_projection;
        glm::vec4 light_position;
        glm::vec4 eye;
    };

    template<typename Collection>
    auto byte_size(const Collection& c) { return sizeof(c.front()) * c.size(); }

    constexpr uint32_t window_width = 1440;
    constexpr uint32_t window_height = 900;
    constexpr uint32_t max_frames_in_flight = 1;
    constexpr float light_range = 50.0f;
    constexpr const char* title = "point shadow map test";

    vkz::vulkan_app_create_info create_info{
        .width = window_height,
        .height = window_height,
        .title = title,
        .depth_testing = true,
        .multiview = true,
    };
}

class psm_app : public vkz::application {
public:
    psm_app() : vkz::application{create_info}
    {}

    void setup() override {
        load_mesh();
        create_descriptor_pool();
        upload_mesh();
        setup_psm();
        create_pipelines();
        setup_camera();
    }

    ~psm_app() {
        vkDeviceWaitIdle(device_);
        render_pipeline.destroy();
        if (psm_id) vkz::psm::destroy(psm_id);
        descriptor_pool.destroy();
        mesh_buffers.vertex_buffer.destroy();
        if (mesh_buffers.index_buffer) mesh_buffers.index_buffer->destroy();
        scene_buffer.destroy();
        shadow_sampler.destroy();
        scene_layout.destroy();
        capture_layout.destroy();
    }

    void setup_psm() {
        capture_layout.device = device_;
        VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &capture_layout.handle));
        const auto& position = mesh.streams.attributes.at(0);
        psm_id = vkz::psm::create({
            .device = device_,
            .depth_format = depth_format(),
            .memory_allocator = allocator_,
            .vertex_shader_include = "layout(location = 0) in vec3 position; mat4 get_model_matrix() { return mat4(1); }",
            .vertex_shader_position_offset = position.offset,
            .vertex_shader_position_stride = mesh.streams.bindings.at(0).stride,
            .vertex_include_descriptor_set_layout = capture_layout,
            .descriptor_pool = descriptor_pool,
            .in_flight_frames = max_frames_in_flight,
            .queue_family_index = app_.queue_family_index(),
            .range = light_range
        });

        scene_layout = vkz::make_descriptor_set_layout_builder(device_)
                           .binding(0)
                           .descriptor_type(vkz::descriptor_type_storage_buffer)
                           .descriptor_count(1)
                           .shader_stages(vkz::shader_stage_vertex | vkz::shader_stage_fragment)
                           .binding(1)
                           .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                           .descriptor_count(1)
                           .shader_stages(vkz::shader_stage_fragment)
                           .create_layout();

        scene_buffer = vkz::buffer::builder(allocator_)
            .size(sizeof(scene_constants))
            .usage(vkz::buffer_usage_storage)
            .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .build();
        shadow_sampler = vkz::sampler::builder(device_).mag_filter(VK_FILTER_LINEAR).min_filter(VK_FILTER_LINEAR)
            .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE).build();
        scene_set = descriptor_pool.allocate(scene_layout);
        const auto shadow = vkz::psm::get_shadow_map(psm_id, 0);
        vkz::update_descriptors(device_, {{.descriptor_set = scene_set,
                                           .bindings = {vkz::descriptor<vkz::buffer_descriptor>(scene_buffer, 0),
                                                        vkz::texture_descriptor{shadow.image_view, shadow_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1}}}});
    }

    void setup_camera() {
        input.bind();
        camera.position = {0, 1.7f, 5};
        camera.acceleration = {18, 18, 18};
        camera.velocity = {8, 8, 8};
        camera.rotationSpeed = 0.15f;
        camera.horizontal_fov = true;
        vkz::camera::first_person movement{camera};
        movement.look_at(camera.position, {0, 1.7f, 0}, {0, 1, 0});
        movement.perspective(65, float(swapchain_->width()) / swapchain_->height(), 0.05f, 1000);
    }

    void create_descriptor_pool() {
        descriptor_pool = {
            context_.device, 100, {{static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 100}, {static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 100}}};
    }

    void load_mesh() {
        auto src = R"(C:\Users\Josiah Ebhomenye\CLionProjects\vulkanizer\test\resources\shadowscene_fire.gltf)";
        auto result = vkz::mesh::convert(src, R"(C:\temp\shadow_scene_fire)");
        vkz::mesh::load_mesh_data(result.mesh_file, mesh);
        vkz::mesh::load_mesh_data_materials(result.material_file, mesh);
    }

    void upload_mesh() {
        auto capacity = byte_size(mesh.index_data) + byte_size(mesh.vertex_data);
        vkz::staging_buffer staging{allocator_, capacity};

        auto vertex_mem = staging.borrow(byte_size(mesh.vertex_data));
        vertex_mem.copy_from(mesh.vertex_data.data());

        auto index_mem = staging.borrow(byte_size(mesh.index_data));
        index_mem.copy_from(mesh.index_data.data());

        mesh_buffers.vertex_buffer = vkz::buffer::builder(allocator_)
            .size(byte_size(mesh.vertex_data))
            .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
            .usage(vkz::buffer_usage_vertex_upload)
            .build();

        mesh_buffers.index_buffer =
            vkz::buffer::builder(allocator_)
                .size(byte_size(mesh.index_data))
                .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                .usage(vkz::buffer_usage_index | vkz::buffer_usage_transfer_dst)
                .build();

        {
            auto command_buffer = vkz::fenced_command_pools::scoped_cmd{*commands_pool_};
            vertex_mem.copy_to(command_buffer, mesh_buffers.vertex_buffer);
            index_mem.copy_to(command_buffer, *mesh_buffers.index_buffer);
        }
        staging.return_memory(vertex_mem);
        staging.return_memory(index_mem);
        staging.destroy();
    }

    void create_pipelines() {
        render_pipeline = vkz::make_graphics_pipeline_builder(device_)
                              .shader_stage()
                              .vertex_shader(VKZ_PSM_TEST_SHADER_DIR "/render.vert.spv")
                              .fragment_shader(VKZ_PSM_TEST_SHADER_DIR "/render.frag.spv")
                              .add_specialization(light_range, 1)
                              .vertex_input_state()
                              .add_vertex_binding_descriptions(mesh.streams.bindings)
                              .add_vertex_attribute_descriptions(mesh.streams.attributes)
                              .input_assembly_state()
                              .triangles()
                              .viewport_state()
                              .viewport()
                              .origin(0, 0)
                              .dimension(window_width, window_width)
                              .min_depth(0)
                              .max_depth(1)
                              .scissor()
                              .offset(0, 0)
                              .extent(window_width, window_height)
                              .add()
                              .rasterization_state()
                              .cull_back_face()
                              .polygon_mode_fill()
                              .depth_stencil_state()
                              .enable_depth_test()
                              .enable_depth_write()
                              .compare_op_less()
                              .color_blend_state()
                              .attachment()
                              .add()
                              .dynamic_render_pass()
                              .add_color_attachment(color_format())
                              .depth_attachment(depth_format())
                              .layout()
                              .add_descriptor_set_layout(scene_layout)
                              .add_push_constant_range(vkz::shader_stage_vertex, 0, sizeof(render_constants))
                              .name("psm_render")
                              .build();
    }

    void app_logic(VkCommandBuffer command_buffer) override {
        vkz::psm::update(psm_id, light_position);
        vkz::psm::capture(psm_id, command_buffer, 0, [&](vkz::pipeline pipeline) {
            vkz::bind_mesh(command_buffer, mesh_buffers);
            for (const auto& instance : mesh.instances) {
                const auto& item = mesh.meshes.at(instance.mesh_id);
                const glm::mat4 model = scene_transform * instance.model;
                vkz::push_constants(command_buffer, pipeline, model);
                vkCmdDrawIndexed(command_buffer, item.get_lod_indices_count(0), 1,
                    item.index_offset + item.lod_offset[0], static_cast<int32_t>(item.vertex_offset), 0);
            }
        });
    }

    void render_swapchain(VkCommandBuffer command_buffer) override {
        vkz::bind_pipeline(command_buffer, render_pipeline);
        vkz::bind_descriptor_sets(command_buffer, render_pipeline, {scene_set});
        vkz::bind_mesh(command_buffer, mesh_buffers);
        for (const auto& instance : mesh.instances) {
            const auto& item = mesh.meshes.at(instance.mesh_id);
            const auto material_id = instance.material_id == vkz::mesh::inherit_material
                    ? item.material_id : instance.material_id;
            const render_constants constants{
                scene_transform * instance.model,
                mesh.materials.at(material_id).base_color_factor
            };
            vkz::push_constants(command_buffer, render_pipeline, constants);
            vkCmdDrawIndexed(command_buffer, item.get_lod_indices_count(0), 1,
                             item.index_offset + item.lod_offset[0], static_cast<int32_t>(item.vertex_offset), 0);
        }
        application::render_swapchain(command_buffer);
    }

    void new_frame() override {
        application::new_frame();
        vkz::camera::first_person{camera}.perspective(camera.fov,
                float(swapchain_->width()) / swapchain_->height(), camera.znear, camera.zfar);
        controller.process_input();
        controller.update(ImGui::GetIO().DeltaTime);
        elapsed_time += ImGui::GetIO().DeltaTime;
        // Reference light motion, transformed by the same scene transform as the meshes.
        const float angle = glm::radians(elapsed_time * 360.0f);
        light_position = glm::vec3(scene_transform * glm::vec4(std::sin(angle) * 0.15f, 1.5f, std::cos(angle) * 0.15f, 1));
        const scene_constants constants{camera.projection * camera.view, glm::vec4(light_position, 1), glm::vec4(camera.position, 1)};
        auto mapping = scene_buffer.map();
        *mapping.as<scene_constants>() = constants;
        scene_buffer.flush();
        mapping.unmap();
    }

    void end_frame() override {
        application::end_frame();
    }

private:
    glm::mat4 scene_transform{1};
    glm::vec3 light_position{0, 1.5f, 0};
    float elapsed_time{};
    vkz::descriptor_set_layout capture_layout;
    vkz::descriptor_set_layout scene_layout;
    vkz::descriptor_set scene_set;
    vkz::buffer scene_buffer;
    vkz::sampler shadow_sampler;
    vkz::camera::camera camera;
    vkz::glfw_input_adaptor input{app_.window()};
    vkz::camera::controller controller{camera, vkz::camera::movement_type::first_person, input.get_device()};
    vkz::psm::id psm_id{};
    vkz::mesh::data mesh;
    vkz::mesh_buffers mesh_buffers;
    vkz::descriptor_pool descriptor_pool;
    vkz::pipeline render_pipeline;
};

int main(int argc, char** argv) {
    vkz::iostream_adapter::install(std::cout);

    try {
        psm_app app{};
        app.main_loop();
    }  catch (const std::exception& error) {
        std::cerr << "Texture test failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
