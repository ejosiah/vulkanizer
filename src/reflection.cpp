#include <vulkanizer/reflection.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/descriptor_set_builder.hpp>
#include <vulkanizer/compute_pipeline_builder.hpp>
#include <vulkanizer/detail/compute_pipeline_layout_builder.hpp>
#include <vulkanizer/detail/compute_shader_stage_builder.hpp>
#include <vulkanizer/ray_tracing_pipeline_builder.hpp>
#include <vulkanizer/reference.hpp>

#include <array>
#include <algorithm>
#include <optional>
#include <cstring>
#include <cmath>
#include <stdexcept>

#include "reflection_reflection_rgen.hpp"
#include "reflection_reflection_rmiss.hpp"
#include "reflection_reflection_rchit.hpp"
#include "reflection_reflection_rahit.hpp"
#include "reflection_temporal_comp.hpp"
#include "reflection_wavelet_comp.hpp"

namespace vkz {
    namespace {
        constexpr auto shader_stages = vkz::pipeline_stage_ray_tracing_shader | vkz::pipeline_stage_compute_shader;

        enum texture_index : size_t {
            raw_color, integrated_color, filtered_0, filtered_1,
            history_color_0, history_color_1, history_moments_0, history_moments_1,
            history_world_0, history_world_1, history_normal_0, history_normal_1,
            integrated_moments, texture_count
        };

        struct constants {
            glm::mat4 inverse_view_projection{1};
            glm::mat4 previous_view_projection{1};
            glm::vec3 camera_position{};
            uint32_t frame_index{};
            glm::uvec2 resolution{};
            uint32_t num_lights{};
            uint32_t vertex_stride{};
            uint32_t history_valid{};
            uint32_t enabled{1};
        };

        static_assert(sizeof(constants) == 168);

        template<size_t N>
        std::vector<uint32_t> shader_words(const uint32_t (&words)[N]) {
            return {words, words + N};
        }

        buffer make_buffer(vma_memory_allocator &allocator, vkz::buffer_usage usage, VkDeviceSize size, const void *data = nullptr) {
            auto result =
                buffer::builder(allocator)
                    .size(size)
                    .usage(usage)
                    .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .build();

            if (data) {
                auto mapping = result.map();
                std::memcpy(mapping.as<void>(), data, size_t(size));
                result.flush();
                mapping.unmap();
            }

            return result;
        }

        void synchronize(VkCommandBuffer cmd) {
            barrier::push_and_flush(cmd, vkz::pipeline_stage_all_commands, shader_stages | vkz::pipeline_stage_transfer, vkz::access_memory_read_write,
                                    vkz::access_shader_read | vkz::access_shader_write | vkz::access_transfer_write);
        }
    }

    struct reflection::implementation {
        std::optional<scene_input> input;
        frame_info frame;
        constants data;
        buffer constants_buffer, light_buffer, instances_buffer, materials_buffer;
        sampler input_sampler;
        std::array<texture, texture_count> textures{};
        std::array<descriptor_set_layout, 4> layouts{};
        descriptor_pool pool;
        std::array<descriptor_set, 4> sets{};
        ray_tracing_pipeline rt_pipeline;
        std::array<pipeline, 3> compute_pipelines{};
        bool initialized{};
        bool previous_enabled{true};
    };

    reflection::reflection(vkz::device device, vma_memory_allocator &allocator,
        std::span<const mesh::light> lights, const texture &depth_buffer, const texture &normal_buffer,
        const texture &surface_buffer, const texture &mesh_id_buffer, glm::uvec2 screen_resolution, const ddgi &gi, bool half_resolution)
        : state_(std::make_unique<implementation>()), device_(device), allocator_(allocator), lights_(lights),
          depth_buffer_(depth_buffer), normal_buffer_(normal_buffer), surface_buffer_(surface_buffer),
          mesh_id_buffer_(mesh_id_buffer), screen_resolution_(screen_resolution), half_resolution_(half_resolution), gi_(gi) {}

    reflection::~reflection() {
        if (state_->input)
            batch_submission{state_->input->queue}.wait_idle();

        state_->rt_pipeline.destroy();

        for (auto &value : state_->compute_pipelines)
            value.destroy();

        state_->pool.destroy();

        for (auto &value : state_->layouts)
            value.destroy();

        for (auto &value : state_->textures)
            value.destroy();

        state_->input_sampler.destroy();
        state_->constants_buffer.destroy();
        state_->light_buffer.destroy();
        state_->instances_buffer.destroy();
        state_->materials_buffer.destroy();
    }

    void reflection::init(const scene_input &scene) {
        if (state_->initialized)
            throw std::logic_error("Reflection is already initialized");

        if (!scene.tlas || !scene.queue || scene.material_textures.empty() || !screen_resolution_.x || !screen_resolution_.y)
            throw std::invalid_argument("Reflection requires scene geometry, material textures and a valid resolution");

        state_->input.emplace(scene);
        state_->data.resolution = (screen_resolution_ + (half_resolution_ ? 1u : 0u)) / (half_resolution_ ? 2u : 1u);
        state_->data.num_lights = uint32_t(lights_.size());
        state_->data.vertex_stride = reference::vertex_stride(scene.scene);

        create_buffers();
        init_textures();
        create_descriptor_set_layouts();
        create_descriptor_pool();
        update_descriptors();
        create_rt_pipeline();
        create_compute_pipelines();
        initialize_resources();

        state_->initialized = true;
    }

    void reflection::create_buffers() {
        const auto &input = *state_->input;
        state_->constants_buffer = make_buffer(allocator_, vkz::buffer_usage_uniform_upload, sizeof(constants));
        state_->light_buffer = make_buffer(allocator_, vkz::buffer_usage_storage_upload, std::max(size_t(1), lights_.size()) * sizeof(reference::light_uniform));
        const auto instances = reference::make_ray_tracing_instances(input.scene);
        state_->instances_buffer = make_buffer(allocator_, vkz::buffer_usage_storage, instances.size() * sizeof(instances.front()), instances.data());
        std::vector<reference::material_uniform> materials;

        for (const auto &material : input.scene.materials)
            materials.push_back(reference::make_material_uniform(input.scene, material));

        state_->materials_buffer = make_buffer(allocator_, vkz::buffer_usage_storage, materials.size() * sizeof(materials.front()), materials.data());
        state_->input_sampler =
            sampler::builder(device_)
                .mag_filter(VK_FILTER_NEAREST)
                .min_filter(VK_FILTER_NEAREST)
                .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
                .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
            .build();
    }

    void reflection::init_textures() {
        for (size_t i = 0; i < state_->textures.size(); ++i) {
            auto &value = state_->textures[i];
            const auto format = i == history_world_0 || i == history_world_1 || i == history_moments_0
                || i == history_moments_1 || i == integrated_moments ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R16G16B16A16_SFLOAT;
            value.image = image::builder(allocator_)
                .format(format)
                .extent(state_->data.resolution.x, state_->data.resolution.y)
                .usage(vkz::image_usage_sampled_storage_transfer)
                .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                .build();

            value.image_view = image_view::builder(device_)
                .image(value.image)
                .format(format)
                .aspect_mask(vkz::image_aspect_color)
                .level_count(1)
                .layer_count(1)
                .build();

            value.sampler =
                sampler::builder(device_)
                    .mag_filter(VK_FILTER_LINEAR)
                    .min_filter(VK_FILTER_LINEAR)
                    .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                .build();
        }
    }

    void reflection::create_descriptor_set_layouts() {
        constexpr auto stages =
            vkz::shader_stage_fragment | vkz::shader_stage_compute | vkz::shader_stage_raygen | vkz::shader_stage_miss | vkz::shader_stage_closest_hit | vkz::shader_stage_any_hit;
        state_->layouts[0] = descriptor_set_layout_builder{device_}
                                 .binding(0)
                                 .descriptor_type(vkz::descriptor_type_uniform_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(1)
                                 .descriptor_type(vkz::descriptor_type_storage_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(2)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(3)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(4)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(5)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(6)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(7)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(8)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(9)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(10)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(11)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(12)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(13)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(14)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(15)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(16)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(17)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(18)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .create_layout();

        state_->layouts[1] = descriptor_set_layout_builder{device_}
                                 .binding(0)
                                 .descriptor_type(vkz::descriptor_type_storage_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(1)
                                 .descriptor_type(vkz::descriptor_type_storage_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(2)
                                 .descriptor_type(vkz::descriptor_type_storage_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(3)
                                 .descriptor_type(vkz::descriptor_type_storage_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(4)
                                 .descriptor_type(vkz::descriptor_type_acceleration_structure)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(5)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                 .descriptor_count(uint32_t(state_->input->material_textures.size()))
                                 .shader_stages(stages)
                                 .create_layout();

        state_->layouts[2] = descriptor_set_layout_builder{device_}
                                 .binding(0)
                                 .descriptor_type(vkz::descriptor_type_uniform_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(1)
                                 .descriptor_type(vkz::descriptor_type_storage_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .create_layout();

        state_->layouts[3] = descriptor_set_layout_builder{device_}
                                 .binding(3)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(4)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(5)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .create_layout();
    }

    void reflection::create_descriptor_pool() {
        state_->pool = descriptor_pool{device_,
                                       4,
                                       {{static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 2},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 6},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), uint32_t(texture_count)},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), uint32_t(state_->input->material_textures.size()) + 7},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_acceleration_structure), 1}}};
        const auto sets = state_->pool.allocate(state_->layouts);
        std::copy(sets.begin(), sets.end(), state_->sets.begin());
    }

    void reflection::update_descriptors() {
        const auto &input = *state_->input;
        const auto gi = gi_.sampling_resources();
        descriptor_bindings frame{.descriptor_set = state_->sets[0],
                                  .bindings = {descriptor<ubo_descriptor>(state_->constants_buffer, 0), descriptor<buffer_descriptor>(state_->light_buffer, 1),
                                               texture_descriptor{depth_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2},
                                               texture_descriptor{normal_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3},
                                               texture_descriptor{surface_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 4},
                                               texture_descriptor{mesh_id_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 5}}};

        for (uint32_t i = 0; i < texture_count; ++i)
            frame.bindings.push_back(image_descriptor{state_->textures[i].image_view, VK_IMAGE_LAYOUT_GENERAL, i + 6});

        update_descriptor(device_, std::move(frame));
        texture_array_descriptor textures;
        textures.binding = 5;

        for (const auto &value : input.material_textures)
            textures.push_back(descriptor<texture_descriptor>(value, 5));

        vkz::update_descriptors(
            device_, {{.descriptor_set = state_->sets[1],
                       .bindings = {descriptor<buffer_descriptor>(input.geometry.vertices, 0), descriptor<buffer_descriptor>(input.geometry.indices, 1),
                                    descriptor<buffer_descriptor>(state_->instances_buffer, 2), descriptor<buffer_descriptor>(state_->materials_buffer, 3), textures}},
                      {.descriptor_set = state_->sets[2], .bindings = {descriptor<ubo_descriptor>(gi.constants, 0), descriptor<buffer_descriptor>(gi.status, 1)}},
                      {.descriptor_set = state_->sets[3],
                       .bindings = {descriptor<texture_descriptor>(gi.irradiance, 3), descriptor<texture_descriptor>(gi.visibility, 4), descriptor<texture_descriptor>(gi.offset, 5)}}});

        // Acceleration structures have no descriptor_t helper yet.
        VkWriteDescriptorSetAccelerationStructureKHR acceleration{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        acceleration.accelerationStructureCount = 1;
        acceleration.pAccelerationStructures = &input.tlas;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.pNext = &acceleration;
        write.dstSet = state_->sets[1].handle;
        write.dstBinding = 4;
        write.descriptorType = static_cast<VkDescriptorType>(vkz::descriptor_type_acceleration_structure);
        write.descriptorCount = 1;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }

    void reflection::create_rt_pipeline() {
        state_->rt_pipeline =
            ray_tracing_pipeline_builder{allocator_}
                .shader_groups()
                    .ray_generation_group("reflection")
                        .ray_generation_shader(shader_words(reflection_reflection_rgen))
                    .miss_group("miss")
                        .miss_shader(shader_words(reflection_reflection_rmiss))
                    .triangles_hit_group("surface")
                        .closest_hit_shader(shader_words(reflection_reflection_rchit))
                            .add_specialization(uint32_t(state_->input->material_textures.size()), 0)
                        .any_hit_shader(shader_words(reflection_reflection_rahit))
                            .add_specialization(uint32_t(state_->input->material_textures.size()), 0)
                .layout()
                    .add_descriptor_set_layouts(state_->layouts)
                .max_recursion_depth(1)
                .name("reflection_rt")
            .build();
    }

    void reflection::create_compute_pipelines() {
        const std::array shaders{shader_words(reflection_temporal_comp), shader_words(reflection_temporal_comp), shader_words(reflection_wavelet_comp)};

        for (size_t i = 0; i < shaders.size(); ++i) {
            state_->compute_pipelines[i] = compute_pipeline_builder{device_}
                                               .shader_stage()
                                               .compute_shader(shaders[i])
                                               .add_specialization(i == 1 ? 1u : 0u, 1)
                                               .layout()
                                               .add_descriptor_set_layouts(state_->layouts)
                                               .add_push_constant_range(vkz::shader_stage_compute, 0, sizeof(uint32_t))
                                               .build();
        }
    }

    void reflection::initialize_resources() {
        const auto &input = *state_->input;
        command_pool uploads{device_, input.queue_family, vkz::command_pool_flags_transient, input.queue};
        const auto cmd = uploads.create_command_buffer();

        for (auto &value : state_->textures) {
            clear(cmd, value.image, VkClearColorValue{}, {});
            barrier::push_and_flush(cmd, value.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_transfer, shader_stages,
                                    vkz::access_transfer_write, vkz::access_shader_read_write, VK_IMAGE_LAYOUT_GENERAL);
        }

        state_->rt_pipeline.sbt.upload(cmd);
        uploads.submit_and_wait(cmd);
    }

    void reflection::new_frame(const frame_info &frame) {
        state_->frame = frame;
        state_->data.inverse_view_projection = frame.inverse_view_projection;
        state_->data.camera_position = frame.camera_position;
        state_->data.enabled = frame.enabled ? (frame.use_ddgi ? 1u : 2u) : 0u;

        if (state_->previous_enabled != frame.enabled)
            state_->data.history_valid = 0;

        state_->previous_enabled = frame.enabled;
    }

    void reflection::exec(VkCommandBuffer cmd) {
        if (!state_->initialized)
            throw std::logic_error("Reflection is not initialized");

        synchronize(cmd);
        update(cmd, state_->constants_buffer, state_->data);
        std::vector<reference::light_uniform> lights;

        for (const auto &light : lights_)
            lights.push_back({glm::vec4(light.position, light.range), glm::vec4(light.direction, float(light.type)),
                glm::vec4(light.color * light.intensity, 0),
                {std::cos(light.inner_cone_angle), std::cos(light.outer_cone_angle), light.source_radius, 0}});

        if (!lights.empty())
            update(cmd, state_->light_buffer, lights.size() * sizeof(lights.front()), lights.data());

        synchronize(cmd);
        bind_pipeline(cmd, state_->rt_pipeline);
        bind_descriptor_sets(cmd, state_->rt_pipeline, state_->sets);
        trace_ray(cmd, state_->rt_pipeline, state_->data.resolution.x, state_->data.resolution.y, "reflection");
        synchronize(cmd);

        if (!state_->frame.denoise) {
            barrier::push_and_flush(cmd, shader_stages, vkz::pipeline_stage_all_commands, vkz::access_shader_write, vkz::access_shader_read);
            return;
        }

        const auto &temporal = state_->compute_pipelines[0];
        bind_pipeline(cmd, temporal);
        bind_descriptor_sets(cmd, temporal, state_->sets);
        dispatch(cmd, (state_->data.resolution.x + 7) / 8, (state_->data.resolution.y + 7) / 8);
        synchronize(cmd);

        const auto &variance = state_->compute_pipelines[1];
        bind_pipeline(cmd, variance);
        bind_descriptor_sets(cmd, variance, state_->sets);
        dispatch(cmd, (state_->data.resolution.x + 7) / 8, (state_->data.resolution.y + 7) / 8);
        synchronize(cmd);

        const auto &wavelet = state_->compute_pipelines[2];
        bind_pipeline(cmd, wavelet);
        bind_descriptor_sets(cmd, wavelet, state_->sets);

        for (uint32_t i = 0; i < 5; ++i) {
            push_constants(cmd, wavelet, i, vkz::shader_stage_compute);
            dispatch(cmd, (state_->data.resolution.x + 7) / 8, (state_->data.resolution.y + 7) / 8);
            synchronize(cmd);
        }

        barrier::push_and_flush(cmd, shader_stages, vkz::pipeline_stage_fragment_shader, vkz::access_shader_write, vkz::access_shader_read);
    }

    void reflection::end_frame() {
        state_->data.previous_view_projection = state_->frame.view_projection;
        ++state_->data.frame_index;
        state_->data.history_valid = state_->frame.enabled && state_->frame.denoise;
    }

    const texture &reflection::raw_texture() const {
        return state_->textures[raw_color];
    }

    reflection::tracing_input reflection::tracing_resources() const {
        return {{state_->layouts[0], state_->layouts[1]}, {state_->sets[0], state_->sets[1]}};
    }

    const texture &reflection::reflection_texture() const {
        return state_->textures[state_->frame.denoise ? filtered_0 : raw_color];
    }
}
