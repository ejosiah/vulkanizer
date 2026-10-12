#define VKZ_IOSTREAM_ADAPTER
#include <vulkanizer/application.hpp>
#include <vulkanizer/graphics_pipeline_builder.hpp>
#include <vulkanizer/ktx.hpp>
#include <vulkanizer/ddgi.hpp>
#include <vulkanizer/shadow.hpp>
#include <vulkanizer/reflection.hpp>
#include <vulkanizer/profiler.hpp>
#include <vulkanizer/descriptor_set_builder.hpp>
#include "rtx_material.hpp"
#include "rtx_tv.hpp"
#include "rtx_nrc.hpp"
#include "rtx_nrd.hpp"
#include <NrcVk.h>
#include "../timeout.hpp"
#include <cmrc/cmrc.hpp>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <numeric>
#include <cmath>
#include <glm/gtc/packing.hpp>

CMRC_DECLARE(rtx_shaders);

class rtx_test final : public vkz::application {
public:
    rtx_test(int argc, char **argv)
        : application(create_info(argc, argv)),
          timeout_(argc, argv) {
        for (int i = 1; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--video-path") {
                if (++i >= argc || std::string_view(argv[i]).empty() || std::string_view(argv[i]).starts_with("--"))
                    throw std::invalid_argument("--video-path requires a file path");

                video_path_ = argv[i];
                continue;
            }

            if (std::string_view(argv[i]) == "--gi-multiplier") {
                if (++i >= argc)
                    throw std::invalid_argument("--gi-multiplier requires a value");
                gi_multiplier_ = std::stof(argv[i]);
                if (!std::isfinite(gi_multiplier_) || gi_multiplier_ < 0)
                    throw std::invalid_argument("--gi-multiplier requires a finite, non-negative value");
                continue;
            }

            profile_gpu_ |= std::string_view(argv[i]) == "--profile";
            if (std::string_view(argv[i]) == "--living-room")
                selected_scene_ = requested_scene_ = 1;
            gi_mode_ = std::string_view(argv[i]) == "--nrc" ? 1 : gi_mode_;
            if (std::string_view(argv[i]) == "--full-resolution") requested_full_resolution_.fill(true);
            requested_full_resolution_[0] |= std::string_view(argv[i]) == "--full-resolution-gi";
            requested_full_resolution_[1] |= std::string_view(argv[i]) == "--full-resolution-shadows";
            requested_full_resolution_[2] |= std::string_view(argv[i]) == "--full-resolution-reflections";
            enable_probe_offsetting_ |= std::string_view(argv[i]) == "--probe-offsetting";
            if (std::string_view(argv[i]) == "--no-probe-offsetting") enable_probe_offsetting_ = false;
            cycle_resolution_ |= std::string_view(argv[i]) == "--cycle-resolution";
            enable_nrd_ |= std::string_view(argv[i]) == "--nrd";
            verify_lighting_ |= std::string_view(argv[i]) == "--verify-lighting";
            cycle_denoising_ |= std::string_view(argv[i]) == "--cycle-denoising";
            visualize_probes_ |= std::string_view(argv[i]) == "--visualize-probes";
            cycle_scenes_ |= std::string_view(argv[i]) == "--cycle-scenes";
        }
    }

    ~rtx_test() {
        vkDeviceWaitIdle(device_);

        if (profile_gpu_) {
            for (const auto &[name, stats] : gpu_profiler_.query_stats())
                std::cout << name << ": " << stats.median << " ms median GPU\n";
        }

        std::cout << "CPU frame recording: " << record_time_ / std::max(1u, total_frames_) << " ms\n";
        std::cout << "Average FPS: " << total_frames_ / std::chrono::duration<double>(std::chrono::steady_clock::now() - run_start_).count() << "\n";
        gpu_profiler_.deinit();
        unload_scene();

        fallback_tv_parameters_.destroy();
        ray_allocator_.destroy();
    }

    void setup() override {
        ray_allocator_ = vkz::vma_memory_allocator::create(context_, VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);

        fallback_tv_parameters_ = load_buffer(vkz::buffer::usage::uniform, std::array<float, 20>{}.data(), 80);
        full_resolution_ = requested_full_resolution_;
        previous_probe_offsetting_ = enable_probe_offsetting_;
        load_scene();

        {
            gpu_profiler_.init(device_);
            gpu_profiler_.external_reset = true;

            for (const auto *name : {"gbuffer", "shadows", "ddgi", "lighting", "presentation", "reflection", "nrc", "nrd"})
                gpu_profiler_.add_query(name);
        }

        run_start_ = std::chrono::steady_clock::now();
    }

    void new_frame() override {
        if (tv_light_)
            tv_light_->new_frame();
    }

    void app_logic(VkCommandBuffer cmd) override {
        if (total_frames_ > 0) {
            gpu_profiler_.commit(frame_queries_);

            for (auto &[name, query] : gpu_profiler_.queries) {
                if (query.runtimes.size() > 240)
                    query.runtimes.erase(query.runtimes.begin());
            }
        }

        if (cycle_scenes_ && total_frames_ == 2)
            requested_scene_ = 1;
        if (cycle_scenes_ && total_frames_ == 4)
            requested_scene_ = 0;

        if (selected_scene_ != requested_scene_) {
            vkz::batch_submission{app_.graphics_queue()}.wait_idle();
            unload_scene();
            selected_scene_ = requested_scene_;
            load_scene();

            for (auto &[name, query] : gpu_profiler_.queries)
                query.runtimes.clear();
        }

        if (cycle_resolution_ && total_frames_ == 2) requested_full_resolution_ = {true, false, true};
        if (cycle_resolution_ && total_frames_ == 8) requested_full_resolution_ = {false, true, false};


        gpu_profiler_.reset_all(cmd);

        const auto record_start = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();

        vkz::rtx::scene_uniform frame;
        frame.vertex_layout = {int(vkz::rtx::vertex_stride(scene_)), 0, 0, 0};
        ImGui::SetNextWindowSizeConstraints({360, 260}, {float(screen_size_.x), float(screen_size_.y)});
        ImGui::Begin("Lighting");
        ImGui::Combo("Scene", &requested_scene_, "Sponza\0Living room\0");

        const auto light_name = "Light " + std::to_string(selected_light_ + 1);
        if (ImGui::BeginCombo("Scene light", light_name.c_str())) {
            for (int i = 0; i < int(lights_.size()); ++i) {
                const auto name = "Light " + std::to_string(i + 1);
                if (ImGui::Selectable(name.c_str(), selected_light_ == i))
                    selected_light_ = i;
            }
            ImGui::EndCombo();
        }

        bool lights_changed = false;
        bool light_enabled = lights_[selected_light_].intensity != 0;
        if (ImGui::Checkbox("Light enabled", &light_enabled)) {
            lights_[selected_light_].intensity = light_enabled ? light_intensities_[selected_light_] : 0;
            lights_changed = true;
        }

        ImGui::Separator();
        ImGui::Combo("GI method", &gi_mode_, "DDGI\0NRC\0");
        ImGui::Checkbox("Full resolution GI", &requested_full_resolution_[0]);
        ImGui::Checkbox("Full resolution shadows", &requested_full_resolution_[1]);
        ImGui::Checkbox("Full resolution reflections", &requested_full_resolution_[2]);
        ImGui::Checkbox("Enable GI", &enable_ddgi_);
        ImGui::Checkbox("NRD denoising", &enable_nrd_);
        ImGui::Checkbox("Visualize probes", &visualize_probes_);
        ImGui::Checkbox("Probe offsetting", &enable_probe_offsetting_);
        ImGui::Checkbox("Enable shadows", &enable_shadows_);
        ImGui::Checkbox("Enable reflections", &enable_reflection_);
        if (ImGui::Checkbox("Indirect light only", &indirect_only_) && indirect_only_)
            reflection_only_ = false;
        if (ImGui::Checkbox("View reflection only", &reflection_only_) && reflection_only_)
            indirect_only_ = false;
        ImGui::SliderFloat("GI multiplier", &gi_multiplier_, 0, 20, "%.2fx");
        ImGui::SliderFloat("Exposure (EV)", &exposure_ev_, -3, 6);
        ImGui::Separator();
        ImGui::TextUnformatted("GPU timings | last 240 frames");
        const auto timings = gpu_profiler_.query_stats();

        if (ImGui::BeginTable("gpu_timings", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("Mean ms");
            ImGui::TableSetupColumn("Median ms");
            ImGui::TableHeadersRow();

            for (const auto *name : {"ddgi", "nrc", "shadows", "reflection", "nrd"}) {
                const auto timing = timings.find(name);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", timing != timings.end() ? timing->second.mean : 0);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", timing != timings.end() ? timing->second.median : 0);
            }

            ImGui::EndTable();
        }

        ImGui::End();

        if (full_resolution_ != requested_full_resolution_ || previous_probe_offsetting_ != enable_probe_offsetting_) {
            vkz::batch_submission{app_.graphics_queue()}.wait_idle();
            full_resolution_ = requested_full_resolution_;
            previous_probe_offsetting_ = enable_probe_offsetting_;
            reload_lighting();
        }


        const auto &io = ImGui::GetIO();
        if (ImGui::IsAnyItemActive() || io.WantCaptureMouse || io.WantCaptureKeyboard) {
            auto &input_device = input_->get_device();
            input_device.mouse.relative_position = {};
            input_device.mouse.scroll_offset = {};
            for (auto &[mapping, button] : input_device.mappings)
                button.pressed = false;
        } else {
            controller_->process_input();
            controller_->update(std::min(0.1f, std::chrono::duration<float>(now - previous_).count()));
        }
        previous_ = now;

        frame.view_projection = camera_.projection * camera_.view;
        frame.eye = glm::vec4(camera_.position, 1);
        frame.num_lights = uint32_t(lights_.size());
        frame.enable_ddgi = enable_ddgi_;
        frame.indirect_only = indirect_only_;
        frame.exposure = std::exp2(exposure_ev_);
        frame.gi_multiplier = gi_multiplier_;
        frame.enable_shadows = enable_shadows_;
        frame.enable_reflection = enable_reflection_;
        frame.reflection_only = reflection_only_;
        frame.render_resolution = {render_size_.x, render_size_.y};
        write_buffer(scene_buffer_, &frame, sizeof(frame));
        const auto light_uniforms = vkz::rtx::make_light_uniforms(lights_);
        write_buffer(light_buffer_, light_uniforms.data(), light_uniforms.size() * sizeof(vkz::rtx::light_uniform));

        if (cycle_denoising_) {
            if (total_frames_ == 2) { gi_mode_ = 1; enable_nrd_ = true; }
            if (total_frames_ == 20) enable_nrd_ = false;
            if (total_frames_ == 25) { gi_mode_ = 0; enable_nrd_ = true; }
        }

        if (gi_mode_ == 1 && !nrc_)
            load_nrc();
        if (lights_changed && nrc_)
            nrc_->reset(scene_bounds_);
        if (enable_nrd_ && !nrd_)
            load_nrd();
        frame_queries_ = {"gbuffer", "shadows", "reflection", "lighting", "presentation"};
        if (gi_mode_ == 0 || visualize_probes_)
            frame_queries_.push_back("ddgi");
        if (gi_mode_ == 1)
            frame_queries_.push_back("nrc");
        if (enable_nrd_)
            frame_queries_.push_back("nrd");

        const bool reset_nrd = !previous_nrd_ || previous_gi_mode_ != gi_mode_ || lights_changed
            || previous_reflection_ != enable_reflection_;

        reflections_->new_frame({glm::inverse(frame.view_projection), frame.view_projection, camera_.position,
            enable_reflection_, !enable_nrd_, gi_mode_ == 0});
        update_lighting_descriptors();

        gpu_profiler_.profile(
            "gbuffer", cmd,
            [&] {
                render_gbuffer(cmd);
            },
            vkz::pipeline::stage::all_commands);
        shadows_->new_frame({glm::inverse(frame.view_projection), frame.view_projection, camera_.position, !enable_nrd_});
        gpu_profiler_.profile(
            "shadows", cmd,
            [&] {
                shadows_->exec(cmd);
            },
            vkz::pipeline::stage::all_commands);

        ddgi_->new_frame({glm::inverse(frame.view_projection), camera_.position});
        if (gi_mode_ == 0 || visualize_probes_)
            gpu_profiler_.profile(
                "ddgi", cmd,
                [&] {
                    ddgi_->exec(cmd);
                },
                vkz::pipeline::stage::all_commands);
        gpu_profiler_.profile(
            "reflection", cmd,
            [&] {
                reflections_->exec(cmd);
            },
            vkz::pipeline::stage::all_commands);

        if (gi_mode_ == 1)
            gpu_profiler_.profile(
                "nrc", cmd,
                [&] {
                    nrc_->exec(cmd);
                },
                vkz::pipeline::stage::all_commands);
        if (enable_nrd_) {
            gpu_profiler_.profile(
                "nrd", cmd,
                [&] {
                    const auto inputs = nrd_inputs();
                    nrd_->exec(cmd, camera_, inputs, reset_nrd);
                    nrd_reflection_->exec(cmd, camera_, inputs, reset_nrd);
                    for (auto &denoiser : nrd_shadows_)
                        denoiser->exec(cmd, camera_, inputs, reset_nrd);
                },
                vkz::pipeline::stage::all_commands);
        }
        previous_nrd_ = enable_nrd_;
        previous_gi_mode_ = gi_mode_;
        previous_reflection_ = enable_reflection_;

        gpu_profiler_.profile(
            "lighting", cmd,
            [&] {
                render_lighting(cmd);
            },
            vkz::pipeline::stage::all_commands);
        record_time_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - record_start).count();
    }

    void render_swapchain(VkCommandBuffer cmd) override {
        vkz::set_viewport(cmd, {0, 0, float(screen_size_.x), float(screen_size_.y), 0, 1});
        vkz::set_scissor(cmd, {{0, 0}, screen_size_});

        vkz::bind_pipeline(cmd, tone_mapping_pipeline_);
        vkz::bind_descriptor_sets(cmd, tone_mapping_pipeline_, {scene_set_});

        const uint32_t srgb_attachment = color_format() == VK_FORMAT_B8G8R8A8_SRGB || color_format() == VK_FORMAT_R8G8B8A8_SRGB;
        vkz::push_constants(cmd, tone_mapping_pipeline_, srgb_attachment, vkz::shader_stage::fragment);
        gpu_profiler_.profile(
            "presentation", cmd,
            [&] {
                vkz::draw(cmd, 3);
            },
            vkz::pipeline::stage::all_commands);

        application::render_swapchain(cmd);
    }

    void frame_submitted() override {
        if (tv_light_)
            tv_light_->frame_submitted();

        if (nrc_)
            nrc_->end_frame(app_.graphics_queue());
    }

    void end_frame() override {
        reflections_->end_frame();
        shadows_->end_frame();
        ddgi_->end_frame();

        ++fps_frames_;
        ++total_frames_;
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration<float>(now - fps_start_).count();

        if (elapsed >= 0.5f) {
            const auto title = (selected_scene_ == 0 ? "Sponza rtx test | " : "Living room rtx test | ") + std::string{} + std::to_string(int(std::round(fps_frames_ / elapsed))) + " FPS";
            glfwSetWindowTitle(app_.window(), title.c_str());
            fps_frames_ = 0;
            fps_start_ = now;
        }

        if (timeout_.expired() || glfwGetKey(app_.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (verify_lighting_)
                verify_lighting();
            glfwSetWindowShouldClose(app_.window(), GLFW_TRUE);
        }
    }

private:
    void load_scene() {
        load_model();
        load_tv();
        load_camera();
        load_lights();
        load_buffers();
        load_textures();
        load_gbuffer();
        load_ddgi();
        load_shadows();
        load_reflections();
        load_denoising_textures();
        load_descriptor_set_layouts();
        load_descriptor_pool();
        load_descriptors();
        load_pipelines();

    }

    void unload_scene() {
        nrd_shadows_.clear();
        nrd_.reset();
        nrd_reflection_.reset();
        nrc_.reset();
        nrd_diffuse_.destroy();
        nrd_specular_.destroy();
        nrd_shadow_.destroy();
        previous_nrd_ = false;
        reflections_.reset();
        shadows_.reset();
        ddgi_.reset();
        acceleration_.reset();

        for (auto &pipeline : gbuffer_pipelines_) pipeline.destroy();
        scene_depth_.destroy();
        scene_normal_.destroy();
        scene_color_.destroy();
        scene_surface_.destroy();
        scene_mesh_id_.destroy();
        tone_mapping_pipeline_.destroy();
        probe_pipeline_.destroy();

        for (auto &pipeline : pipelines_) pipeline.destroy();
        descriptor_pool_.destroy();

        for (auto &layout : layouts_) layout.destroy();

        for (auto &buffer : material_buffers_) buffer.destroy();

        scene_buffer_.destroy();
        light_buffer_.destroy();
        vertices_.destroy();
        indices_.destroy();

        fallback_tv_.image_view.destroy();
        fallback_tv_ = {};

        if (tv_light_)
            textures_.pop_back(); // Borrowed persistent screen belongs to tv_light.

        for (auto &texture : textures_) texture.destroy();

        tv_light_.reset();


        material_buffers_.clear();
        material_sets_.clear();
        textures_.clear();
        scene_ = {};
    }

    void draw_scene(VkCommandBuffer cmd, bool gbuffer) {
        vkz::set_viewport(cmd, {0, 0, float(render_size_.x), float(render_size_.y), 0, 1});
        vkz::set_scissor(cmd, {{0, 0}, render_size_});
        vkz::bind_index_buffer<uint32_t>(cmd, indices_);

        auto order = draw_order_;
        std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
            const auto &ia = scene_.instances[a], &ib = scene_.instances[b];
            const bool ba = scene_.materials[vkz::rtx::material_index(scene_, ia)].alpha_mode == vkz::mesh::alpha_mode::blend;
            const bool bb = scene_.materials[vkz::rtx::material_index(scene_, ib)].alpha_mode == vkz::mesh::alpha_mode::blend;
            if (ba != bb) return !ba;
            if (!ba) return false;
            const auto depth = [&](const auto &instance) {
                return -(camera_.view * instance.model * glm::vec4(scene_.boxes.at(instance.mesh_id).get_center(), 1)).z;
            };
            return depth(ia) > depth(ib);
        });

        for (const auto id : order) {
            const auto &instance = scene_.instances[id];
            const auto &mesh = scene_.meshes.at(instance.mesh_id);
            const auto material = vkz::rtx::material_index(scene_, instance);
            const auto pipeline_id = (scene_.materials[material].alpha_mode == vkz::mesh::alpha_mode::blend ? 2 : 0)
                + (glm::determinant(glm::mat3(instance.model)) < 0 ? 1 : 0);

            if (gbuffer && (pipeline_id & 2))
                continue;

            const auto &pipeline = gbuffer ? gbuffer_pipelines_[pipeline_id & 1] : pipelines_[pipeline_id];
            vkz::bind_pipeline(cmd, pipeline);
            vkz::bind_descriptor_sets(cmd, pipeline, {scene_set_, material_sets_[material], reflections_->tracing_resources().sets[1]});
            vkz::push_constants(cmd, pipeline, vkz::rtx::instance_uniform{instance.model, glm::transpose(instance.model_inverse)}, vkz::shader_stage::vertex | vkz::shader_stage::fragment);
            vkz::draw_indexed(cmd, mesh.get_lod_indices_count(0), 1, mesh.index_offset + mesh.lod_offset[0], int32_t(mesh.vertex_offset), id);
        }
    }

    static vkz::vulkan_app_create_info create_info(int argc, char **argv) {
        vkz::vulkan_app_create_info info{.width = 1440, .height = 1280,
            .title = "Sponza rtx test", .resizable = false, .validation = false,
            .vsync = false ,
            .enabled_features = {.shader_storage_image_extended_formats = true}};

        for (int i = 1; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--validation")
                info.validation = true;

            if (std::string_view(argv[i]) == "--no-validation")
                info.validation = false;
        }

        info.queues.push_back(vkz::queue::video_decode);
        info.extension_chain.add(VkPhysicalDeviceSamplerYcbcrConversionFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES, nullptr, VK_TRUE});

        info.device_extensions = {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };

        for (const auto extension : vkz::video::required_extensions())
            info.device_extensions.push_back(extension);

        const char *const *extensions;
        auto extension_count = nrc::vulkan::GetVulkanDeviceExtensions(extensions);
        for (uint32_t i = 0; i < extension_count; ++i) {
            if (std::none_of(info.device_extensions.begin(), info.device_extensions.end(), [&](const char *name) {
                return std::string_view(name) == extensions[i];
            }))
                info.device_extensions.push_back(extensions[i]);
        }
        extension_count = nrc::vulkan::GetVulkanInstanceExtensions(extensions);
        for (uint32_t i = 0; i < extension_count; ++i)
            info.instance_extensions.push_back(extensions[i]);
        info.enabled_features.shader_storage_image_read_without_format = true;
        info.enabled_features.shader_storage_image_write_without_format = true;
        VkPhysicalDeviceTimelineSemaphoreFeatures timeline{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
        timeline.timelineSemaphore = VK_TRUE;
        VkPhysicalDeviceUniformBufferStandardLayoutFeatures uniform{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES};
        uniform.uniformBufferStandardLayout = VK_TRUE;

        VkPhysicalDeviceBufferDeviceAddressFeatures address{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
        address.bufferDeviceAddress = VK_TRUE;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
        acceleration.accelerationStructure = VK_TRUE;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR tracing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
        tracing.rayTracingPipeline = VK_TRUE;

        VkPhysicalDeviceDescriptorIndexingFeatures indexing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
        indexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

        VkPhysicalDeviceScalarBlockLayoutFeatures scalar{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES};
        scalar.scalarBlockLayout = VK_TRUE;
        VkPhysicalDeviceRayQueryFeaturesKHR ray_query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
        ray_query.rayQuery = VK_TRUE;

        info.extension_chain
            .add(address)
            .add(acceleration)
            .add(tracing)
            .add(ray_query)
            .add(indexing)
            .add(scalar)
            .add(timeline)
            .add(uniform);

        return info;
    }

    void load_model() {
        const auto mesh_path = selected_scene_ == 0 ? VKZ_SPONZA_MESH : VKZ_LIVING_ROOM_MESH;
        vkz::mesh::load_mesh_data(mesh_path, scene_);
        auto materials = vkz::path{mesh_path};
        materials.replace_extension(".vkmat");
        vkz::mesh::load_mesh_data_materials(materials, scene_);
        if (scene_.instances.empty() || scene_.materials.empty()) throw std::runtime_error("Scene requires mesh instances and materials");

        if (selected_scene_ == 1) {
            // Material 5 is the ceiling lamp shell in LivingRoom.vkmat.
            constexpr uint32_t ceiling_lamp_material = 5;
            auto &material = scene_.materials.at(ceiling_lamp_material);

            // Materials 20-22 are the three cushion fabrics in LivingRoom.vkmat.
            for (const uint32_t pillow_material : {20u, 21u, 22u}) {
                auto &pillow = scene_.materials.at(pillow_material);
                pillow.metallic_factor = material.metallic_factor;
                pillow.roughness_factor = material.roughness_factor;
                pillow.metallic_roughness_texture = material.metallic_roughness_texture;
            }
        }

        draw_order_.resize(scene_.instances.size());
        std::iota(draw_order_.begin(), draw_order_.end(), 0);
    }

    void write_buffer(vkz::buffer &buffer, const void *data, size_t size) {
        auto mapping = buffer.map();
        std::memcpy(mapping.as<void>(), data, size);
        buffer.flush();
        mapping.unmap();
    }

    void load_lights() {
        selected_light_ = 0;
        if (selected_scene_ == 1) {
            lights_.clear();
            for (auto &light : scene_.lights) {
                if (light.type == vkz::mesh::light_type::point && lights_.size() < 2)
                    lights_.push_back(light);
            }
            if (lights_.size() != 2)
                throw std::runtime_error("Living room requires its two authored point lights");
        } else {
            // Precomputed with spectrum::blackbodySpectrum({5000, 1}): Planck's law,
            // peak-normalized spectrum, CIE XYZ integration, then its XYZ_TO_RGB matrix.
            constexpr glm::vec3 color_5000_k{1.00118284f, 0.97817062f, 0.84464176f};
            lights_ = {{.type = vkz::mesh::light_type::point,
                        // The reference scales the blackbody RGB and light intensity separately.
                        .color = color_5000_k * 10.0f,
                        .intensity = 10.0f,
                        .range = 5.0f,
                        .position = {0, 1, -2}}};
        }

        light_intensities_.clear();
        for (const auto &light : lights_)
            light_intensities_.push_back(light.intensity);
    }

    vkz::buffer load_buffer(vkz::buffer::usage usage, const void *data, size_t size) {
        auto buffer =
            vkz::buffer::builder(ray_allocator_)
                .usage(usage)
                .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .size(size)
            .build();

        if (data)
            write_buffer(buffer, data, size);

        return buffer;
    }

    void load_buffers() {
        constexpr auto geometry_usage = vkz::buffer::usage::acceleration_storage_input;

        vertices_ = load_buffer(geometry_usage, scene_.vertex_data.data(), scene_.vertex_data.size());
        indices_ = load_buffer(geometry_usage | vkz::buffer::usage::index, scene_.index_data.data(), scene_.index_data.size() * sizeof(uint32_t));

        scene_buffer_ = load_buffer(vkz::buffer::usage::uniform, nullptr, sizeof(vkz::rtx::scene_uniform));
        const auto lights = vkz::rtx::make_light_uniforms(lights_);
        light_buffer_ = load_buffer(vkz::buffer::usage::storage, lights.data(), lights.size() * sizeof(vkz::rtx::light_uniform));
        for (const auto &material : scene_.materials) {
            const auto uniform = vkz::rtx::make_material_uniform(scene_, material);
            material_buffers_.push_back(load_buffer(vkz::buffer::usage::uniform, &uniform, sizeof(uniform)));
        }
    }

    void load_tv() {
        if (selected_scene_ != 1)
            return;

        const auto file = cmrc::rtx_shaders::get_filesystem().open("tv_filter.comp.spv");
        std::vector<uint32_t> shader(file.size() / sizeof(uint32_t));
        std::memcpy(shader.data(), file.begin(), file.size());
        tv_light_ = std::make_unique<rtx::tv_light>(allocator_, app_.graphics_queue(), app_.queue_family_index(),
            video_path_, shader);
        tv_light_->configure(scene_);
    }

    void load_textures() {
        textures_.resize(1);
        auto &white = textures_.front();
        white.image = vkz::image::builder(allocator_)
            .format(VK_FORMAT_R8G8B8A8_UNORM)
            .extent(1, 1)
            .usage(vkz::image::usage::sampled_upload)
            .build();

        white.image_view = vkz::image_view::builder(device_)
            .image(white.image)
            .format(VK_FORMAT_R8G8B8A8_UNORM)
            .aspect_mask(vkz::image::aspect::color)
            .level_count(1)
            .layer_count(1)
            .build();

        white.sampler =
            vkz::sampler::builder(device_)
            .build();

        vkz::command_pool uploads{device_, app_.queue_family_index(), vkz::command_pool_flags::transient, app_.graphics_queue()};
        const auto cmd = uploads.create_command_buffer();
        vkz::clear(cmd, white.image, VkClearColorValue{{1, 1, 1, 1}}, {});
        vkz::barrier::push_and_flush(cmd, white.image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1}, vkz::pipeline::stage::transfer, vkz::pipeline::stage::fragment_shader,
                                     vkz::access::transfer_write, vkz::access::shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        uploads.submit_and_wait(cmd);

        fallback_tv_.image_view.destroy();
        fallback_tv_ = white;
        fallback_tv_.image_view = vkz::image_view::builder(device_)
                                      .image(white.image)
                                      .view_type(VK_IMAGE_VIEW_TYPE_2D_ARRAY)
                                      .format(VK_FORMAT_R8G8B8A8_UNORM)
                                      .subresource_range({static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1})
                                      .build();

        for (size_t i = 0; i < scene_.textures.size(); ++i) {
            if (tv_light_ && i + 1 == scene_.textures.size()) {
                textures_.push_back(tv_light_->screen());
                continue;
            }

            const auto &texture = scene_.textures[i];
            const auto settings = texture.sampler_id < 0 ? vkz::mesh::sampler{} : scene_.samplers.at(texture.sampler_id);
            textures_.push_back(vkz::texture_from_ktx(scene_.images.at(texture.image_id).encoded_data,
                allocator_, app_.graphics_queue(), app_.queue_family_index(), settings));
        }
    }

    void load_descriptor_set_layouts() {
        layouts_[0] = vkz::descriptor_set_layout_builder{device_}
                          .binding(0)
                          .descriptor_type(vkz::descriptor::type::uniform_buffer)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::vertex | vkz::shader_stage::fragment)
                          .binding(1)
                          .descriptor_type(vkz::descriptor::type::storage_buffer)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::vertex)
                          .binding(2)
                          .descriptor_type(vkz::descriptor::type::storage_buffer)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(3)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(4)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(5)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(6)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(7)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(8)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::vertex)
                          .binding(9)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(10)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(11)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(12)
                          .descriptor_type(vkz::descriptor::type::uniform_buffer)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .create_layout();

        layouts_[1] = vkz::descriptor_set_layout_builder{device_}
                          .binding(0)
                          .descriptor_type(vkz::descriptor::type::uniform_buffer)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::vertex | vkz::shader_stage::fragment)
                          .binding(1)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(2)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(3)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(4)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(5)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(6)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(7)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(8)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(9)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(10)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(11)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(12)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(13)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(14)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(15)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(16)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .binding(17)
                          .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                          .descriptor_count(1)
                          .shader_stages(vkz::shader_stage::fragment)
                          .create_layout();
    }

    void load_descriptor_pool() {
        const auto count = uint32_t(scene_.materials.size());
        descriptor_pool_ = vkz::descriptor_pool{device_,
                                                count + 1,
                                                {{static_cast<VkDescriptorType>(vkz::descriptor::type::uniform_buffer), count + 2},
                                                 {static_cast<VkDescriptorType>(vkz::descriptor::type::storage_buffer), 2},
                                                 {static_cast<VkDescriptorType>(vkz::descriptor::type::combined_image_sampler), count * uint32_t(vkz::mesh::material_texture_count) + 11}}};
    }

    void load_descriptors() {
        scene_set_ = descriptor_pool_.allocate(layouts_[0]);
        material_sets_ = descriptor_pool_.allocate_n(layouts_[1], scene_.materials.size());

        vkz::update_descriptor(
            device_,
            {.descriptor_set = scene_set_,
             .bindings = {vkz::make_descriptor<vkz::ubo_descriptor>(scene_buffer_, 0), vkz::make_descriptor<vkz::buffer_descriptor>(vertices_, 1),
                          vkz::make_descriptor<vkz::buffer_descriptor>(light_buffer_, 2), vkz::make_descriptor<vkz::texture_descriptor>(ddgi_->indirect_light_texture(), 3),
                          vkz::texture_descriptor{scene_color_.image_view, scene_color_.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 4},
                          vkz::make_descriptor<vkz::texture_descriptor>(shadows_->visibility_texture(), 5), vkz::make_descriptor<vkz::texture_descriptor>(reflections_->reflection_texture(), 6),
                          vkz::make_descriptor<vkz::texture_descriptor>(ddgi_->irradiance_atlas(), 7), vkz::make_descriptor<vkz::texture_descriptor>(ddgi_->sampling_resources().offset, 8)}});

        vkz::update_descriptor(device_, {.descriptor_set = scene_set_,
                                         .bindings = {vkz::make_descriptor<vkz::texture_descriptor>(tv_light_ ? tv_light_->filtered() : fallback_tv_, 9),
                                                      vkz::make_descriptor<vkz::texture_descriptor>(tv_light_ ? tv_light_->ltc_matrix() : textures_.front(), 10),
                                                      vkz::make_descriptor<vkz::texture_descriptor>(tv_light_ ? tv_light_->ltc_amplitude() : textures_.front(), 11),
                                                      vkz::make_descriptor<vkz::ubo_descriptor>(tv_light_ ? tv_light_->parameters() : fallback_tv_parameters_, 12)}});

        for (size_t i = 0; i < scene_.materials.size(); ++i) {
            vkz::descriptor_bindings bindings{.descriptor_set = material_sets_[i], .bindings = {vkz::make_descriptor<vkz::ubo_descriptor>(material_buffers_[i], 0)}};

            const auto slots = vkz::mesh::texture_slots(scene_.materials[i]);

            for (uint32_t slot = 0; slot < slots.size(); ++slot) {
                const auto &texture = textures_.at(slots[slot]->index + 1);
                bindings.bindings.push_back(vkz::make_descriptor<vkz::texture_descriptor>(texture, slot + 1));
            }

            vkz::update_descriptor(device_, std::move(bindings));
        }
    }

    void load_gbuffer() {
        const auto create = [&](vkz::texture &target, VkFormat format, vkz::image::usage usage, vkz::image::aspect aspect) {
            target.image = vkz::image::builder(allocator_)
                .format(format)
                .extent(render_size_.x, render_size_.y)
                .usage(usage | vkz::image::usage::sampled)
                .build();

            target.image_view =
                vkz::image_view::builder(device_)
                    .image(target.image)
                    .format(format)
                    .aspect_mask(aspect)
                    .level_count(1)
                    .layer_count(1)
                    .build();
        };

        create(scene_depth_, VK_FORMAT_D32_SFLOAT, vkz::image::usage::depth_stencil, vkz::image::aspect::depth);
        create(scene_normal_, VK_FORMAT_R16G16_SFLOAT, vkz::image::usage::color, vkz::image::aspect::color);
        create(scene_surface_, VK_FORMAT_R16G16B16A16_SFLOAT, vkz::image::usage::color, vkz::image::aspect::color);
        create(scene_mesh_id_, VK_FORMAT_R32_UINT, vkz::image::usage::color, vkz::image::aspect::color);
        create(scene_color_, VK_FORMAT_R16G16B16A16_SFLOAT, vkz::image::usage::color_download, vkz::image::aspect::color);

        scene_color_.sampler =
            vkz::sampler::builder(device_)
            .build();
    }

    void load_ddgi() {
        if (!acceleration_) {
            acceleration_ = std::make_unique<vkz::mesh::bvh>(ray_allocator_);
            vkz::command_pool uploads{device_, app_.queue_family_index(), vkz::command_pool_flags::transient, app_.graphics_queue()};
            const auto cmd = uploads.create_command_buffer();

            acceleration_->build(cmd, scene_, {vertices_, indices_});
            uploads.submit_and_wait(cmd);
        }

        ddgi_ = std::make_unique<vkz::ddgi>(device_, ray_allocator_, lights_, scene_depth_, scene_normal_, render_size_);

        vkz::ddgi::settings settings;
        // Match the reference volume: unit spacing, centered in X/Z, above the floor.
        settings.grid_position = {-10, 0.5f, -10};
        settings.half_resolution = !full_resolution_[0];
        settings.relocate_probes = enable_probe_offsetting_;

        if (selected_scene_ == 1) {
            auto bounds = scene_.boxes.at(scene_.instances.front().mesh_id).get_transformed(scene_.instances.front().model);
            for (const auto &instance : scene_.instances) {
                const auto box = scene_.boxes.at(instance.mesh_id).get_transformed(instance.model);
                bounds.combine_point(box.min);
                bounds.combine_point(box.max);
            }

            settings.probe_grid.count = {7, 4, 7};
            settings.grid_position = bounds.min + glm::vec3(0.1f);
            settings.probe_grid.spacing = glm::max((bounds.get_size() - glm::vec3(0.2f))
                / glm::vec3(settings.probe_grid.count - glm::ivec3(1)), glm::vec3(0.1f));
            settings.probe_grid.inverse_spacing = 1.0f / settings.probe_grid.spacing;
            settings.self_shadow_bias = 0.05f;
            settings.relocate_probes = enable_probe_offsetting_;
            settings.classify_probes = false;
        }

        probe_parameters_ = {settings.grid_position, settings.probe_grid.count,
            settings.probe_grid.spacing, settings.probe_grid.sphere_scale, int(settings.irradiance_side_length),
            uint32_t(settings.relocate_probes)};
        ddgi_->configure(settings);
        ddgi_->init({scene_, {vertices_, indices_}, acceleration_->tlas().handle, app_.graphics_queue(), app_.queue_family_index(), textures_});
    }

    void load_shadows() {
        shadows_ = std::make_unique<vkz::shadow>(device_, ray_allocator_, lights_, scene_depth_, scene_normal_, render_size_, !full_resolution_[1]);
        shadows_->init({acceleration_->tlas().handle, app_.graphics_queue(), app_.queue_family_index(), &scene_, {vertices_, indices_}, textures_});
    }

    void load_reflections() {
        reflections_ = std::make_unique<vkz::reflection>(device_, ray_allocator_, lights_, scene_depth_, scene_normal_,
            scene_surface_, scene_mesh_id_, render_size_, *ddgi_, !full_resolution_[2]);
        reflections_->init({scene_, {vertices_, indices_}, acceleration_->tlas().handle,
            app_.graphics_queue(), app_.queue_family_index(), textures_});
    }

    void render_gbuffer(VkCommandBuffer cmd) {
        constexpr VkImageSubresourceRange depth_range{static_cast<VkImageAspectFlags>(vkz::image::aspect::depth), 0, 1, 0, 1};
        constexpr VkImageSubresourceRange normal_range{static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1};

        vkz::barrier::push_and_flush(cmd, scene_depth_.image, depth_range, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::early_fragment_tests | vkz::pipeline::stage::late_fragment_tests,
                                     vkz::access::memory_read_write, vkz::access::depth_stencil_attachment_read_write, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        vkz::barrier::push_and_flush(cmd, scene_normal_.image, normal_range, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::color_attachment_output, vkz::access::memory_read_write,
                                     vkz::access::color_attachment_write, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        for (auto *value : {&scene_surface_, &scene_mesh_id_}) {
            vkz::barrier::push_and_flush(cmd, value->image, normal_range, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::color_attachment_output, vkz::access::memory_read_write,
                                         vkz::access::color_attachment_write, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        vkz::render_info info{
            .color_attachments = {
                {scene_normal_.image_view, VK_FORMAT_R16G16_SFLOAT, {0, 0, 0, 0}},
                {scene_surface_.image_view, VK_FORMAT_R16G16B16A16_SFLOAT, {0, 0, 0, 1}},
                {scene_mesh_id_.image_view, VK_FORMAT_R32_UINT, {0, 0, 0, 0}}
            },
            .depth_attachment = vkz::depth_stencil_attachment{scene_depth_.image_view, VK_FORMAT_D32_SFLOAT},
            .render_area = render_size_
        };

        vkz::render(cmd, info, [&] { draw_scene(cmd, true); });

        vkz::barrier::push_and_flush(cmd, scene_depth_.image, depth_range, vkz::pipeline::stage::early_fragment_tests | vkz::pipeline::stage::late_fragment_tests,
                                     vkz::pipeline::stage::compute_shader | vkz::pipeline::stage::ray_tracing_shader, vkz::access::depth_stencil_attachment_write, vkz::access::shader_read,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkz::barrier::push_and_flush(cmd, scene_normal_.image, normal_range, vkz::pipeline::stage::color_attachment_output,
                                     vkz::pipeline::stage::compute_shader | vkz::pipeline::stage::ray_tracing_shader, vkz::access::color_attachment_write, vkz::access::shader_read,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        for (auto *value : {&scene_surface_, &scene_mesh_id_}) {
            vkz::barrier::push_and_flush(cmd, value->image, normal_range, vkz::pipeline::stage::color_attachment_output,
                                         vkz::pipeline::stage::compute_shader | vkz::pipeline::stage::ray_tracing_shader, vkz::access::color_attachment_write, vkz::access::shader_read,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

    void render_lighting(VkCommandBuffer cmd) {
        constexpr VkImageSubresourceRange color_range{static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1};
        constexpr VkImageSubresourceRange depth_range{static_cast<VkImageAspectFlags>(vkz::image::aspect::depth), 0, 1, 0, 1};

        vkz::barrier::push_and_flush(cmd, scene_color_.image, color_range, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::color_attachment_output, vkz::access::memory_read_write,
                                     vkz::access::color_attachment_write, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        vkz::barrier::push_and_flush(cmd, scene_depth_.image, depth_range, vkz::pipeline::stage::compute_shader, vkz::pipeline::stage::early_fragment_tests | vkz::pipeline::stage::late_fragment_tests,
                                     vkz::access::shader_read, vkz::access::depth_stencil_attachment_read_write, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        vkz::render_info info{
            .color_attachments = {{scene_color_.image_view, VK_FORMAT_R16G16B16A16_SFLOAT}},
            .depth_attachment = vkz::depth_stencil_attachment{scene_depth_.image_view, VK_FORMAT_D32_SFLOAT},
            .render_area = render_size_
        };

        if (visualize_probes_) {
            vkz::barrier::push_and_flush(cmd, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::vertex_shader | vkz::pipeline::stage::fragment_shader, vkz::access::memory_write,
                                         vkz::access::shader_read);
        }

        vkz::render(cmd, info, [&] {
            draw_scene(cmd, false);
            if (visualize_probes_) {
                vkz::bind_pipeline(cmd, probe_pipeline_);
                vkz::bind_descriptor_sets(cmd, probe_pipeline_, {scene_set_});
                vkz::push_constants(cmd, probe_pipeline_, probe_parameters_, vkz::shader_stage::vertex | vkz::shader_stage::fragment);
                const auto count = probe_parameters_.counts;
                vkz::draw(cmd, 12 * 8 * 6, count.x * count.y * count.z);
            }
        });

        vkz::barrier::push_and_flush(cmd, scene_color_.image, color_range, vkz::pipeline::stage::color_attachment_output, vkz::pipeline::stage::fragment_shader, vkz::access::color_attachment_write,
                                     vkz::access::shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void load_pipelines() {
        const auto shader = [](const char *name) {
            const auto file = cmrc::rtx_shaders::get_filesystem().open(name);

            if (file.size() % sizeof(uint32_t))
                throw std::runtime_error("Invalid Sponza SPIR-V");

            std::vector<uint32_t> words(file.size() / sizeof(uint32_t));
            std::memcpy(words.data(), file.begin(), file.size());

            return words;
        };

        for (uint32_t i = 0; i < pipelines_.size() + gbuffer_pipelines_.size(); ++i) {
            const bool gbuffer = i >= pipelines_.size();
            vkz::graphics_pipeline_builder builder{device_};

            builder
                .shader_stage()
                    .vertex_shader(shader("sponza.vert.spv"))
                    .fragment_shader(shader(gbuffer ? "gbuffer.frag.spv" : "sponza.frag.spv"))
                        .add_specialization(uint32_t(textures_.size()), 0)
                .input_assembly_state()
                    .triangles()
                .viewport_state()
                    .viewport()
                        .origin(0, 0)
                        .dimension(render_size_.x, render_size_.y)
                        .min_depth(0)
                        .max_depth(1)
                    .scissor()
                        .offset(0, 0)
                        .extent(render_size_.x, render_size_.y)
                        .add()
                .dynamic_state()
                    .viewport()
                    .scissor()
                .rasterization_state()
                    .cull_none()
                    .polygon_mode_fill()
                .multisample_state()
                    .rasterization_samples(vkz::sample_count::x1)
                .depth_stencil_state()
                    .enable_depth_test()
                    .compare_op_less_or_equal();

            if (i & 1)
                builder.rasterization_state().front_face_clockwise();
            else
                builder.rasterization_state().front_face_counter_clockwise();

            if (!gbuffer && (i & 2)) {
                builder
                    .depth_stencil_state()
                        .disable_depth_write()
                    .color_blend_state()
                        .attachment()
                            .enable_blend()
                            .src_color_blend_factor()
                                .src_alpha()
                            .dst_color_blend_factor()
                                .one_minus_src_alpha()
                            .src_alpha_blend_factor()
                                .one()
                            .dst_alpha_blend_factor()
                                .one_minus_src_alpha();
            } else {
                builder.depth_stencil_state().enable_depth_write();
            }

            auto &pipeline = gbuffer ? gbuffer_pipelines_[i - pipelines_.size()] : pipelines_[i];

            builder.color_blend_state()
                .attachment()
                .add()
                .layout()
                .add_descriptor_set_layouts(layouts_)
                .add_descriptor_set_layout(reflections_->tracing_resources().layouts[1])
                .add_push_constant_range(vkz::shader_stage::vertex | vkz::shader_stage::fragment, 0, sizeof(vkz::rtx::instance_uniform))
                .name(gbuffer ? "rtx_sponza_gbuffer" : "rtx_sponza");

            auto &rendering = builder.dynamic_render_pass();
            rendering
                .add_color_attachment(gbuffer ? VK_FORMAT_R16G16_SFLOAT : VK_FORMAT_R16G16B16A16_SFLOAT)
                .depth_attachment(VK_FORMAT_D32_SFLOAT);

            if (gbuffer) {
                builder
                    .color_blend_state()
                        .attachment()
                            .add()
                        .attachment()
                            .add();

                rendering
                    .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
                    .add_color_attachment(VK_FORMAT_R32_UINT);
            }

            pipeline = builder.build();
        }

        tone_mapping_pipeline_ = vkz::graphics_pipeline_builder{device_}
                                     .shader_stage()
                                     .vertex_shader(shader("tone_mapping.vert.spv"))
                                     .fragment_shader(shader("tone_mapping.frag.spv"))
                                     .input_assembly_state()
                                     .triangles()
                                     .viewport_state()
                                     .viewport()
                                     .origin(0, 0)
                                     .dimension(render_size_.x, render_size_.y)
                                     .min_depth(0)
                                     .max_depth(1)
                                     .scissor()
                                     .offset(0, 0)
                                     .extent(render_size_.x, render_size_.y)
                                     .add()
                                     .dynamic_state()
                                     .viewport()
                                     .scissor()
                                     .rasterization_state()
                                     .cull_none()
                                     .polygon_mode_fill()
                                     .multisample_state()
                                     .rasterization_samples(vkz::sample_count::x1)
                                     .depth_stencil_state()
                                     .disable_depth_test()
                                     .disable_depth_write()
                                     .color_blend_state()
                                     .attachment()
                                     .add()
                                     .layout()
                                     .add_descriptor_set_layout(layouts_[0])
                                     .add_push_constant_range(vkz::shader_stage::fragment, 0, sizeof(uint32_t))
                                     .dynamic_render_pass()
                                     .add_color_attachment(color_format())
                                     .depth_attachment(depth_format())
                                     .name("rtx_uncharted2_tone_mapping")
                                     .build();

        probe_pipeline_ = vkz::graphics_pipeline_builder{device_}
                              .shader_stage()
                              .vertex_shader(shader("probes.vert.spv"))
                              .fragment_shader(shader("probes.frag.spv"))
                              .input_assembly_state()
                              .triangles()
                              .viewport_state()
                              .viewport()
                              .origin(0, 0)
                              .dimension(render_size_.x, render_size_.y)
                              .min_depth(0)
                              .max_depth(1)
                              .scissor()
                              .offset(0, 0)
                              .extent(render_size_.x, render_size_.y)
                              .add()
                              .dynamic_state()
                              .viewport()
                              .scissor()
                              .rasterization_state()
                              .cull_none()
                              .polygon_mode_fill()
                              .multisample_state()
                              .rasterization_samples(vkz::sample_count::x1)
                              .depth_stencil_state()
                              .enable_depth_test()
                              .compare_op_less_or_equal()
                              .disable_depth_write()
                              .color_blend_state()
                              .attachment()
                              .add()
                              .layout()
                              .add_descriptor_set_layout(layouts_[0])
                              .add_push_constant_range(vkz::shader_stage::vertex | vkz::shader_stage::fragment, 0, sizeof(probe_parameters))
                              .dynamic_render_pass()
                              .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
                              .depth_attachment(VK_FORMAT_D32_SFLOAT)
                              .name("rtx_probe_visualization")
                              .build();
    }

    void verify_lighting() {
        if (tv_light_) {
            tv_light_->verify();
            if (!indirect_only_ && !reflection_only_)
                tv_light_->verify_lighting(scene_color_, [&](VkCommandBuffer cmd) { render_lighting(cmd); });
        }

        vkz::batch_submission{app_.graphics_queue()}.wait_idle();
        const auto verify = [&](const vkz::texture &texture, const char *name, bool require_light) {
            const auto extent = texture.image.create_info.extent;
            const auto count = size_t(extent.width) * extent.height;
            auto readback = vkz::buffer::builder(ray_allocator_)
                .size(count * sizeof(glm::vec4))
                .usage(vkz::buffer::usage::transfer_dst)
                .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
                .build();
            vkz::command_pool commands{device_, app_.queue_family_index(), vkz::command_pool_flags::transient, app_.graphics_queue()};
            const auto cmd = commands.create_command_buffer();
            auto image = texture.image;
            vkz::copy(cmd, image, readback);
            vkz::barrier::push_and_flush(cmd, image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1}, vkz::pipeline::stage::transfer, vkz::pipeline::stage::all_commands,
                                         vkz::access::transfer_read, vkz::access::shader_read_write, VK_IMAGE_LAYOUT_GENERAL);
            vkz::barrier::gpu_to_cpu(cmd);
            commands.submit_and_wait(cmd);
            vmaInvalidateAllocation(readback.allocator, readback.allocation, 0, VK_WHOLE_SIZE);
            auto mapping = readback.map();
            const auto *values = mapping.as<glm::vec4>();
            double sum = 0;
            float maximum = 0;
            bool finite = true;
            for (size_t i = 0; i < count; ++i) {
                for (int channel = 0; channel < 3; ++channel) {
                    finite &= std::isfinite(values[i][channel]) && values[i][channel] >= 0;
                    sum += values[i][channel];
                    maximum = std::max(maximum, values[i][channel]);
                }
            }
            mapping.unmap();
            readback.destroy();
            std::cout << name << ": mean=" << sum / (count * 3) << ", max=" << maximum << "\n";
            if (!finite || (require_light && maximum <= 0))
                throw std::runtime_error(std::string(name) + " has invalid or empty lighting");
        };
        if (gi_mode_ == 1 && nrc_)
            verify(nrc_->indirect_light_texture(), "NRC raster GI", true);
        if (enable_nrd_) {
            verify(nrd_diffuse_, "NRD diffuse", true);
            verify(nrd_specular_, "NRD reflection", false);
            float raw_minimum = 1, raw_maximum = 0;
            for (const auto *texture : {&shadows_->visibility_texture(), static_cast<const vkz::texture *>(&nrd_shadow_)}) {
                const auto extent = texture->image.create_info.extent;
                const auto count = size_t(extent.width) * extent.height * extent.depth;
                auto readback = vkz::buffer::builder(ray_allocator_)
                    .size(count * sizeof(uint16_t))
                    .usage(vkz::buffer::usage::transfer_dst)
                    .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
                    .build();
                vkz::command_pool commands{device_, app_.queue_family_index(), vkz::command_pool_flags::transient, app_.graphics_queue()};
                const auto cmd = commands.create_command_buffer();
                auto image = texture->image;
                vkz::copy(cmd, image, readback);
                vkz::barrier::push_and_flush(cmd, image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1}, vkz::pipeline::stage::transfer, vkz::pipeline::stage::all_commands,
                                             vkz::access::transfer_read, vkz::access::shader_read_write, VK_IMAGE_LAYOUT_GENERAL);
                vkz::barrier::gpu_to_cpu(cmd);
                commands.submit_and_wait(cmd);
                vmaInvalidateAllocation(readback.allocator, readback.allocation, 0, VK_WHOLE_SIZE);
                auto mapping = readback.map();
                const auto *values = mapping.as<uint16_t>();
                float minimum = 1, maximum = 0;
                bool valid = true;
                for (size_t i = 0; i < count; ++i) {
                    const float visibility = glm::unpackHalf1x16(values[i]);
                    valid &= std::isfinite(visibility) && visibility >= 0 && visibility <= 1;
                    minimum = std::min(minimum, visibility);
                    maximum = std::max(maximum, visibility);
                }
                mapping.unmap();
                readback.destroy();
                const bool raw = texture != &nrd_shadow_;
                std::cout << (raw ? "Raw shadows" : "NRD shadows") << ": min=" << minimum << ", max=" << maximum << "\n";
                if (!valid)
                    throw std::runtime_error("Shadows have invalid visibility");
                if (raw) {
                    raw_minimum = minimum;
                    raw_maximum = maximum;
                } else if (raw_minimum != raw_maximum && minimum == maximum) {
                    throw std::runtime_error("NRD lost shadow visibility variation");
                }
            }
        }
    }

    glm::uvec2 lighting_resolution(size_t effect) const {
        return full_resolution_[effect] ? render_size_ : (render_size_ + 1u) / 2u;
    }

    void reload_lighting() {
        nrd_shadows_.clear();
        nrd_.reset();
        nrd_reflection_.reset();
        nrc_.reset();
        nrd_diffuse_.destroy();
        nrd_specular_.destroy();
        nrd_shadow_.destroy();
        reflections_.reset();
        shadows_.reset();
        ddgi_.reset();
        previous_nrd_ = false;

        load_ddgi();
        load_shadows();
        load_reflections();
        load_denoising_textures();

        vkz::update_descriptor(
            device_, {.descriptor_set = scene_set_,
                      .bindings = {vkz::make_descriptor<vkz::texture_descriptor>(ddgi_->irradiance_atlas(), 7), vkz::make_descriptor<vkz::texture_descriptor>(ddgi_->sampling_resources().offset, 8)}});

        for (auto &[name, query] : gpu_profiler_.queries)
            query.runtimes.clear();
    }

    void load_nrc() {
        nrc_ = std::make_unique<rtx::nrc_gi>(context_, ray_allocator_, *reflections_, uint32_t(textures_.size()),
            lighting_resolution(0), scene_bounds_, app_.graphics_queue(), app_.queue_family_index());
    }

    rtx::nrd_denoiser::input nrd_inputs() {
        const auto &diffuse = gi_mode_ == 1 ? nrc_->indirect_light_texture() : ddgi_->indirect_light_texture();
        return {scene_depth_, scene_normal_, scene_surface_, diffuse, reflections_->raw_texture(), shadows_->penumbra_texture(),
            nrd_diffuse_, nrd_specular_, nrd_shadow_};
    }

    void load_nrd() {
        const auto inputs = nrd_inputs();
        nrd_ = std::make_unique<rtx::nrd_denoiser>(context_, lighting_resolution(0), inputs, -3);
        nrd_reflection_ = std::make_unique<rtx::nrd_denoiser>(context_, lighting_resolution(2), inputs, -2);
        for (uint32_t i = 0; i < lights_.size(); ++i)
            nrd_shadows_.push_back(std::make_unique<rtx::nrd_denoiser>(context_, lighting_resolution(1), inputs, int(i)));
    }

    void load_denoising_textures() {
        const auto create = [&](vkz::texture &texture, bool volume, size_t effect) {
            const auto resolution = lighting_resolution(effect);
            const auto format = volume ? VK_FORMAT_R16_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
            texture.image = vkz::image::builder(ray_allocator_)
                .type(volume ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D)
                .format(format)
                .extent(resolution.x, resolution.y, volume ? uint32_t(lights_.size()) : 1u)
                .usage(vkz::image::usage::sampled_storage_transfer)
                .build();
            texture.image_view = vkz::image_view::builder(device_)
                .image(texture.image)
                .view_type(volume ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D)
                .format(format)
                .aspect_mask(vkz::image::aspect::color)
                .level_count(1)
                .layer_count(1)
                .build();
            texture.sampler =
                vkz::sampler::builder(device_)
                    .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                .build();
        };
        create(nrd_diffuse_, false, 0);
        create(nrd_specular_, false, 2);
        create(nrd_shadow_, true, 1);
        vkz::command_pool uploads{device_, app_.queue_family_index(), vkz::command_pool_flags::transient, app_.graphics_queue()};
        const auto cmd = uploads.create_command_buffer();
        for (auto *texture : {&nrd_diffuse_, &nrd_specular_, &nrd_shadow_}) {
            vkz::clear(cmd, texture->image, VkClearColorValue{}, {});
            vkz::barrier::push_and_flush(cmd, texture->image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1}, vkz::pipeline::stage::transfer,
                                         vkz::pipeline::stage::all_commands, vkz::access::transfer_write, vkz::access::shader_read_write, VK_IMAGE_LAYOUT_GENERAL);
        }
        uploads.submit_and_wait(cmd);
    }

    void update_lighting_descriptors() {
        const auto &diffuse = enable_nrd_ ? nrd_diffuse_
            : gi_mode_ == 1 ? nrc_->indirect_light_texture() : ddgi_->indirect_light_texture();
        vkz::update_descriptor(device_, {.descriptor_set = scene_set_,
                                         .bindings = {vkz::make_descriptor<vkz::texture_descriptor>(diffuse, 3),
                                                      vkz::make_descriptor<vkz::texture_descriptor>(enable_nrd_ ? nrd_shadow_ : shadows_->visibility_texture(), 5),
                                                      vkz::make_descriptor<vkz::texture_descriptor>(enable_nrd_ ? nrd_specular_ : reflections_->reflection_texture(), 6)}});
    }

    void load_camera() {
        auto bounds = scene_.boxes.at(scene_.instances.front().mesh_id).get_transformed(scene_.instances.front().model);
        for (const auto &instance : scene_.instances) {
            const auto box = scene_.boxes.at(instance.mesh_id).get_transformed(instance.model);
            bounds.combine_point(box.min);
            bounds.combine_point(box.max);
        }
        const auto center = bounds.get_center();
        scene_bounds_ = glm::vec4(center, glm::length(bounds.get_size()) * 0.5f);
        camera_.position = {center.x + bounds.get_size().x * 0.3f, bounds.min.y + 1.7f, center.z};
        camera_.acceleration = {18, 18, 18};
        camera_.velocity = {8, 8, 8};
        camera_.rotationSpeed = 0.15f;
        camera_.horizontal_fov = true;
        vkz::camera::first_person initializer(camera_);
        initializer.look_at(camera_.position, {center.x, camera_.position.y, center.z}, {0, 1, 0});
        initializer.perspective(65, 1440.0f / 1280.0f, 0.05f, 1000);
        // Keep the callback chain installed once for the lifetime of the window.
        if (!input_) {
            input_ = std::make_unique<vkz::glfw_input_adaptor>(app_.window(), false);
            input_->bind();
        }

        auto &input_device = input_->get_device();
        input_device.mouse.relative_position = {};
        input_device.mouse.scroll_offset = {};
        for (auto &[mapping, button] : input_device.mappings)
            button.pressed = false;
        controller_ = std::make_unique<vkz::camera::controller>(camera_, vkz::camera::movement_type::first_person, input_->get_device());
        previous_ = std::chrono::steady_clock::now();
    }

    glm::vec4 scene_bounds_{};
    std::unique_ptr<rtx::nrc_gi> nrc_;
    std::unique_ptr<rtx::nrd_denoiser> nrd_;
    std::unique_ptr<rtx::nrd_denoiser> nrd_reflection_;
    std::vector<std::unique_ptr<rtx::nrd_denoiser>> nrd_shadows_;
    vkz::texture nrd_diffuse_, nrd_specular_, nrd_shadow_;
    int gi_mode_{};
    int previous_gi_mode_{};
    std::array<bool, 3> full_resolution_{};
    std::array<bool, 3> requested_full_resolution_{};
    bool cycle_resolution_{};
    bool enable_nrd_{};
    bool previous_nrd_{};
    bool previous_reflection_{true};
    bool cycle_denoising_{};
    bool verify_lighting_{};
    vkz::vma_memory_allocator ray_allocator_;
    std::unique_ptr<vkz::mesh::bvh> acceleration_;
    std::unique_ptr<vkz::ddgi> ddgi_;
    std::unique_ptr<vkz::shadow> shadows_;
    std::unique_ptr<vkz::reflection> reflections_;
    const glm::uvec2 render_size_{1440, 1280};
    vkz::texture scene_depth_, scene_normal_, scene_color_, scene_surface_, scene_mesh_id_;
    struct probe_parameters {
        glm::vec3 grid_position{};
        glm::ivec3 counts{};
        glm::vec3 spacing{};
        float radius{};
        int irradiance_side_length{};
        uint32_t offsetting_enabled{};
    };
    static_assert(sizeof(probe_parameters) == 48);
    probe_parameters probe_parameters_;
    bool visualize_probes_{};
    bool enable_probe_offsetting_{true};
    bool previous_probe_offsetting_{};
    vkz::pipeline probe_pipeline_;
    vkz::pipeline tone_mapping_pipeline_;
    std::array<vkz::pipeline, 2> gbuffer_pipelines_{};

    std::unique_ptr<rtx::tv_light> tv_light_;
    vkz::path video_path_{VKZ_CAT_VIDEO};
    vkz::texture fallback_tv_;
    vkz::buffer fallback_tv_parameters_;

    vkz::mesh::data scene_;
    vkz::buffer vertices_, indices_, scene_buffer_, light_buffer_;
    std::vector<vkz::mesh::light> lights_;
    std::vector<float> light_intensities_;
    int selected_scene_{};
    int requested_scene_{};
    int selected_light_{};
    bool cycle_scenes_{};
    bool reflection_only_{};
    std::vector<vkz::buffer> material_buffers_;
    std::vector<vkz::texture> textures_;
    std::array<vkz::descriptor_set_layout, 2> layouts_{};
    vkz::descriptor_pool descriptor_pool_;
    vkz::descriptor_set scene_set_{};
    std::vector<vkz::descriptor_set> material_sets_;
    std::array<vkz::pipeline, 4> pipelines_{};
    std::vector<uint32_t> draw_order_;
    vkz::camera::camera camera_;
    std::unique_ptr<vkz::glfw_input_adaptor> input_;
    std::unique_ptr<vkz::camera::controller> controller_;
    std::chrono::steady_clock::time_point previous_;
    bool enable_shadows_{true};
    bool enable_reflection_{true};
    uint32_t fps_frames_{};
    std::chrono::steady_clock::time_point fps_start_{std::chrono::steady_clock::now()};
    bool enable_ddgi_{true};
    bool indirect_only_{};
    float exposure_ev_{};
    float gi_multiplier_{1};

    uint32_t total_frames_{};
    double record_time_{};
    std::chrono::steady_clock::time_point run_start_{std::chrono::steady_clock::now()};
    vkz::profiler gpu_profiler_;
    std::vector<std::string> frame_queries_;
    bool profile_gpu_{};
    test_timeout timeout_;
};

int main(int argc, char **argv) try {
    vkz::iostream_adapter::install(std::cout);
    rtx_test app(argc, argv);
    app.main_loop();
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}

