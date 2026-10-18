#include <vulkanizer/ddgi.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/descriptor_set_builder.hpp>
#include <vulkanizer/compute_pipeline_builder.hpp>
#include <vulkanizer/detail/compute_pipeline_layout_builder.hpp>
#include <vulkanizer/detail/compute_shader_stage_builder.hpp>
#include <vulkanizer/ray_tracing_pipeline_builder.hpp>
#include <vulkanizer/reference.hpp>
#include <vulkanizer/ktx.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <limits>
#include <stdexcept>

#include "ddgi_probe_rgen.hpp"
#include "ddgi_probe_rmiss.hpp"
#include "ddgi_probe_rchit.hpp"
#include "ddgi_probe_rahit.hpp"
#include "ddgi_irradiance_update_comp.hpp"
#include "ddgi_visibility_update_comp.hpp"
#include "ddgi_sample_indirect_comp.hpp"
#include "ddgi_update_probes_comp.hpp"

namespace vkz {
    namespace {
        constexpr auto shader_stages = vkz::pipeline_stage_ray_tracing_shader | vkz::pipeline_stage_compute_shader;

        struct constants {
            glm::mat4 random_rotation{1};
            glm::vec3 probe_grid_position{};
            glm::vec3 probe_spacing{1};
            glm::vec3 reciprocal_probe_spacing{1};
            glm::ivec3 probe_counts{};
            int32_t probe_update_offset{};
            int32_t probe_update_count{};
            int32_t probe_rays{};
            int32_t irradiance_side_length{};
            int32_t visibility_side_length{};
            float hysteresis{};
            float self_shadow_bias{};
            float infinite_bounces_multiplier{};
            uint32_t output_resolution_half{};
            uint32_t num_lights{};
            uint32_t ddgi_debug_options{};
            uint32_t vertex_stride{};
            uint32_t frame_index{};
            float max_ray_distance{};
            glm::mat4 inverse_view_projection{1};
            glm::vec3 camera_position{};
            float max_offset{};
            float sphere_scale{};
        };

        static_assert(sizeof(constants) == 252);
        static_assert(offsetof(constants, inverse_view_projection) == 168);
        static_assert(offsetof(constants, probe_counts) == 100);

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
        std::vector<reference::light_uniform> pack_lights(std::span<const mesh::light> lights) {
            std::vector<reference::light_uniform> result;
            for (const auto &light : lights)
                result.push_back({glm::vec4(light.position, light.range), glm::vec4(light.direction, float(light.type)),
                glm::vec4(light.color * light.intensity, 0), {std::cos(light.inner_cone_angle), std::cos(light.outer_cone_angle), light.source_radius, 0}});
            if (result.empty())
                result.emplace_back();

            return result;
        }
        void synchronize(VkCommandBuffer cmd) {
            barrier::push_and_flush(cmd, vkz::pipeline_stage_all_commands, shader_stages | vkz::pipeline_stage_transfer, vkz::access_memory_read_write,
                                    vkz::access_shader_read | vkz::access_shader_write | vkz::access_transfer_write);
        }
    }

    struct ddgi::implementation {
        settings config;

        frame_info frame;
        std::optional<scene_input> input;
        constants data;

        buffer constants_buffer, status_buffer, light_buffer, instances_buffer, materials_buffer;
        sampler input_sampler;
        std::vector<texture> material_textures;
        bool borrowed_material_textures{};

        std::array<descriptor_set_layout, 4> layouts{};
        std::array<VkDescriptorSet, 4> sets{};
        descriptor_pool pool;
        std::array<descriptor_set, 4> descriptor_sets{};

        ray_tracing_pipeline probe_pipeline;
        std::array<pipeline, 4> compute_pipelines{};

        uint32_t light_capacity{};
        uint32_t num_probes{};
        bool initialized{};
    };

    ddgi::ddgi(vkz::device device, vma_memory_allocator &allocator, std::span<const mesh::light> lights,
        const texture &depth_buffer, const texture &normal_buffer, glm::uvec2 screen_resolution)
        : state_(std::make_unique<implementation>()), device_(device), allocator_(allocator), lights_(lights),
          depth_buffer_(depth_buffer), normal_buffer_(normal_buffer), screen_resolution_(screen_resolution) {}

    ddgi::~ddgi() {
        destroy_resources();
    }

    void ddgi::destroy_resources() {
        if (state_->pool.handle() || state_->constants_buffer._)
            vkDeviceWaitIdle(device_);

        state_->probe_pipeline.destroy();

        for (auto &pipeline : state_->compute_pipelines) {
            pipeline.destroy();
            pipeline = {};
        }

        state_->pool.destroy();
        state_->pool = {};

        for (auto &layout : state_->layouts) {
            layout.destroy();
            layout = {};
        }

        for (auto *value : {&state_->constants_buffer, &state_->status_buffer, &state_->light_buffer, &state_->instances_buffer, &state_->materials_buffer})
            value->destroy();

        state_->input_sampler.destroy();

        if (!state_->borrowed_material_textures) {
            for (auto &value : state_->material_textures)
                value.destroy();
        }
        state_->material_textures.clear();
        state_->borrowed_material_textures = false;

        for (auto *value : {&indirect_light, &radiance, &irradiance, &probe_grid_irradiance, &probe_grid_visibility, &probe_offset, &probe_hit_geometry})
            value->destroy();

        state_->initialized = false;
    }

    void ddgi::configure(const settings &value) {
        if (state_->initialized || state_->constants_buffer._)
            throw std::logic_error("Configure DDGI before init");

        const auto &p = value.probe_grid;
        if (glm::any(glm::lessThanEqual(p.count, glm::ivec3(0))) || glm::any(glm::lessThanEqual(p.spacing, glm::vec3(0))) || p.rays_per_probe == 0
            || value.irradiance_side_length == 0 || value.visibility_side_length == 0 || value.hysteresis < 0 || value.hysteresis >= 1 || value.max_ray_distance <= 0
            || p.update_counts < 0 || p.update_offsets < 0 || p.max_offset < 0 || p.max_offset > 0.5f)
            throw std::invalid_argument("Invalid DDGI probe configuration");

        const auto finite = [](const auto &vector) {
            return !glm::any(glm::isnan(vector)) && !glm::any(glm::isinf(vector));
        };

        if (!finite(p.spacing) || !finite(value.grid_position) || !std::isfinite(value.hysteresis)
            || !std::isfinite(value.self_shadow_bias) || value.self_shadow_bias < 0
            || !std::isfinite(value.infinite_bounces_multiplier) || value.infinite_bounces_multiplier < 0
            || !std::isfinite(value.max_ray_distance) || !std::isfinite(p.max_offset))
            throw std::invalid_argument("DDGI settings must be finite");

        const auto num_probes = uint64_t(p.count.x) * uint64_t(p.count.y) * uint64_t(p.count.z);
        if (num_probes > uint64_t(INT32_MAX) || p.rays_per_probe > uint32_t(INT32_MAX))
            throw std::invalid_argument("DDGI probe configuration exceeds shader index limits");

        state_->config = value;
        probes_ = p;
        probes_.inverse_spacing = 1.0f / probes_.spacing;
        half_resolution_ = value.half_resolution;
        irradiance_side_length_ = value.irradiance_side_length;
        visibility_side_length_ = value.visibility_side_length;
    }

    void ddgi::init(const scene_input &scene) {
        if (state_->initialized)
            throw std::logic_error("DDGI is already initialized");

        state_->input.emplace(scene);

        init();
    }

    void ddgi::init() {
        if (state_->initialized)
            throw std::logic_error("DDGI is already initialized");
        if (!state_->input)
            throw std::logic_error("DDGI init requires scene geometry, TLAS and an upload queue");

        const auto &input = *state_->input;

        if (!device_ || !input.tlas || !input.queue || !input.geometry.vertices._ || !input.geometry.indices._
            || input.scene.instances.empty() || input.scene.materials.empty() || !depth_buffer_.image_view.handle || !normal_buffer_.image_view.handle
            || screen_resolution_.x == 0 || screen_resolution_.y == 0 || (half_resolution_ && (screen_resolution_.x < 2 || screen_resolution_.y < 2)))
            throw std::invalid_argument("DDGI requires valid scene and G-buffer inputs");

        for (const auto *value : {&depth_buffer_, &normal_buffer_}) {
            const auto extent = value->image.create_info.extent;
            if (extent.width != screen_resolution_.x || extent.height != screen_resolution_.y
                || !(value->image.create_info.usage & static_cast<VkImageUsageFlags>(vkz::image_usage_sampled)))
                throw std::invalid_argument("DDGI G-buffer textures must be sampled images at screen resolution");
        }
        if (!(input.geometry.vertices.create_info.usage & static_cast<VkBufferUsageFlags>(vkz::buffer_usage_storage))
            || !(input.geometry.indices.create_info.usage & static_cast<VkBufferUsageFlags>(vkz::buffer_usage_storage)))
            throw std::invalid_argument("DDGI geometry buffers require storage buffer usage");

        try {
            init_textures();
            create_buffers();
            create_descriptor_set_layouts();
            create_descriptor_pool();
            update_descriptors();
            create_probe_rt_pipeline();
            init_compute_pipelines();
            initialize_resources();
            state_->initialized = true;
            new_frame();
        } catch (...) {
            destroy_resources();
            throw;
        }
    }

    ddgi::sampling_input ddgi::sampling_resources() const {
        return {state_->constants_buffer, state_->status_buffer, probe_grid_irradiance, probe_grid_visibility, probe_offset};
    }

    void ddgi::create_buffers() {
        const auto &input = *state_->input;

        state_->data.vertex_stride = reference::vertex_stride(input.scene);

        state_->num_probes = uint32_t(probes_.count.x * probes_.count.y * probes_.count.z);
        state_->light_capacity = std::max(1u, uint32_t(lights_.size()));
        if (state_->light_capacity > 1024)
            throw std::invalid_argument("DDGI supports at most 1024 lights per update");

        state_->constants_buffer = make_buffer(allocator_, vkz::buffer_usage_uniform_upload, sizeof(constants));

        std::vector<uint32_t> status(state_->num_probes, 4);
        state_->status_buffer = make_buffer(allocator_, vkz::buffer_usage_storage, status.size() * sizeof(uint32_t), status.data());

        state_->light_buffer = make_buffer(allocator_, vkz::buffer_usage_storage_upload, state_->light_capacity * sizeof(reference::light_uniform));

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
                .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
            .build();

        if (!input.material_textures.empty()) {
            if (input.material_textures.size() != input.scene.textures.size() + 1)
                throw std::invalid_argument("DDGI material textures must include a white fallback and each scene texture");

            state_->borrowed_material_textures = true;
            state_->material_textures.assign(input.material_textures.begin(), input.material_textures.end());
            return;
        }

        texture white;
        white.image = image::builder(allocator_)
            .format(VK_FORMAT_R8G8B8A8_UNORM)
            .extent(1, 1)
            .usage(vkz::image_usage_sampled_upload)
            .build();

        state_->material_textures.push_back(white);

        auto &fallback = state_->material_textures.front();
        fallback.image_view = image_view::builder(device_)
            .image(fallback.image)
            .format(VK_FORMAT_R8G8B8A8_UNORM)
            .aspect_mask(vkz::image_aspect_color)
            .level_count(1)
            .layer_count(1)
            .build();

        fallback.sampler =
            sampler::builder(device_)
            .build();

        for (const auto &texture : input.scene.textures) {
            const auto sampler_settings = texture.sampler_id < 0 ? mesh::sampler{} : input.scene.samplers.at(texture.sampler_id);
            state_->material_textures.push_back(texture_from_ktx(input.scene.images.at(texture.image_id).encoded_data, allocator_, input.queue, input.queue_family, sampler_settings));
        }
    }

    void ddgi::create_descriptor_set_layouts() {
        constexpr auto stages = vkz::shader_stage_compute | vkz::shader_stage_raygen | vkz::shader_stage_miss | vkz::shader_stage_closest_hit | vkz::shader_stage_any_hit;

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
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(3)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(4)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(5)
                                 .descriptor_type(vkz::descriptor_type_storage_image)
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
                                 .create_layout();

        state_->layouts[1] = descriptor_set_layout_builder{device_}
                                 .binding(0)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                 .descriptor_count(1)
                                 .shader_stages(stages)
                                 .binding(1)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
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
                                 .create_layout();

        state_->layouts[2] =
            descriptor_set_layout_builder{device_}.binding(0).descriptor_type(vkz::descriptor_type_storage_buffer).descriptor_count(1).shader_stages(vkz::shader_stage_closest_hit).create_layout();

        state_->layouts[3] = descriptor_set_layout_builder{device_}
                                 .binding(0)
                                 .descriptor_type(vkz::descriptor_type_storage_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(vkz::shader_stage_closest_hit | vkz::shader_stage_any_hit)
                                 .binding(1)
                                 .descriptor_type(vkz::descriptor_type_storage_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(vkz::shader_stage_closest_hit | vkz::shader_stage_any_hit)
                                 .binding(2)
                                 .descriptor_type(vkz::descriptor_type_storage_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(vkz::shader_stage_closest_hit | vkz::shader_stage_any_hit)
                                 .binding(3)
                                 .descriptor_type(vkz::descriptor_type_storage_buffer)
                                 .descriptor_count(1)
                                 .shader_stages(vkz::shader_stage_closest_hit | vkz::shader_stage_any_hit)
                                 .binding(4)
                                 .descriptor_type(vkz::descriptor_type_acceleration_structure)
                                 .descriptor_count(1)
                                 .shader_stages(vkz::shader_stage_raygen)
                                 .binding(5)
                                 .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                 .descriptor_count(uint32_t(state_->material_textures.size()))
                                 .shader_stages(vkz::shader_stage_closest_hit | vkz::shader_stage_any_hit)
                                 .create_layout();
    }

    void ddgi::create_descriptor_pool() {
        state_->pool = descriptor_pool{device_,
                                       4,
                                       {{static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 1},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 6},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), 6},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), uint32_t(state_->material_textures.size()) + 6},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_acceleration_structure), 1}}};
        const auto sets = state_->pool.allocate(state_->layouts);

        for (size_t i = 0; i < sets.size(); ++i) {
            state_->descriptor_sets[i] = sets[i];
            state_->sets[i] = sets[i].handle;
        }
    }

    void ddgi::update_descriptors() {
        texture_array_descriptor material_textures;
        material_textures.binding = 5;

        for (const auto &value : state_->material_textures) {
            material_textures.push_back(texture_descriptor{
                value.image_view, value.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });
        }

        const auto &input = *state_->input;

        vkz::update_descriptors(
            device_, {{.descriptor_set = state_->descriptor_sets[0],
                       .bindings = {descriptor<ubo_descriptor>(state_->constants_buffer, 0), descriptor<buffer_descriptor>(state_->status_buffer, 1),
                                    image_descriptor{probe_grid_irradiance.image_view, VK_IMAGE_LAYOUT_GENERAL, 2}, image_descriptor{probe_grid_visibility.image_view, VK_IMAGE_LAYOUT_GENERAL, 3},
                                    image_descriptor{radiance.image_view, VK_IMAGE_LAYOUT_GENERAL, 4}, image_descriptor{indirect_light.image_view, VK_IMAGE_LAYOUT_GENERAL, 5},
                                    image_descriptor{probe_offset.image_view, VK_IMAGE_LAYOUT_GENERAL, 6}, image_descriptor{probe_hit_geometry.image_view, VK_IMAGE_LAYOUT_GENERAL, 7}}},
                      {.descriptor_set = state_->descriptor_sets[1],
                       .bindings = {texture_descriptor{depth_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0},
                                    texture_descriptor{normal_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1},
                                    texture_descriptor{radiance.image_view, radiance.sampler, VK_IMAGE_LAYOUT_GENERAL, 2},
                                    texture_descriptor{probe_grid_irradiance.image_view, probe_grid_irradiance.sampler, VK_IMAGE_LAYOUT_GENERAL, 3},
                                    texture_descriptor{probe_grid_visibility.image_view, probe_grid_visibility.sampler, VK_IMAGE_LAYOUT_GENERAL, 4},
                                    texture_descriptor{probe_offset.image_view, probe_offset.sampler, VK_IMAGE_LAYOUT_GENERAL, 5}}},
                      {.descriptor_set = state_->descriptor_sets[2], .bindings = {descriptor<buffer_descriptor>(state_->light_buffer, 0)}},
                      {.descriptor_set = state_->descriptor_sets[3],
                       .bindings = {descriptor<buffer_descriptor>(input.geometry.vertices, 0), descriptor<buffer_descriptor>(input.geometry.indices, 1),
                                    descriptor<buffer_descriptor>(state_->instances_buffer, 2), descriptor<buffer_descriptor>(state_->materials_buffer, 3), material_textures}}});

        // Acceleration structures are not supported by descriptor_t yet.
        VkWriteDescriptorSetAccelerationStructureKHR acceleration{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        acceleration.accelerationStructureCount = 1;
        acceleration.pAccelerationStructures = &input.tlas;

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.pNext = &acceleration;
        write.dstSet = state_->sets[3];
        write.dstBinding = 4;
        write.descriptorType = static_cast<VkDescriptorType>(vkz::descriptor_type_acceleration_structure);
        write.descriptorCount = 1;

        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }

    void ddgi::create_probe_rt_pipeline() {
        state_->probe_pipeline =
            ray_tracing_pipeline_builder{allocator_}
                .shader_groups()
                    .ray_generation_group("probes")
                    .ray_generation_shader(shader_words(ddgi_probe_rgen))
                .miss_group("miss")
                    .miss_shader(shader_words(ddgi_probe_rmiss))
                .triangles_hit_group("surface")
                    .closest_hit_shader(shader_words(ddgi_probe_rchit))
                        .add_specialization(uint32_t(state_->material_textures.size()), 0)
                    .any_hit_shader(shader_words(ddgi_probe_rahit))
                        .add_specialization(uint32_t(state_->material_textures.size()), 0)
                .layout()
                    .add_descriptor_set_layouts(state_->layouts)
                .max_recursion_depth(1)
                .name("ddgi_probe_rt")
            .build();

    }

    void ddgi::init_compute_pipelines() {
        const std::array shaders{shader_words(ddgi_irradiance_update_comp), shader_words(ddgi_visibility_update_comp), shader_words(ddgi_sample_indirect_comp), shader_words(ddgi_update_probes_comp)};
        const std::array names{"ddgi_irradiance_update", "ddgi_visibility_update", "ddgi_sample_indirect", "ddgi_update_probes"};
        for (size_t i = 0; i < shaders.size(); ++i) {
            state_->compute_pipelines[i] = compute_pipeline_builder{device_}
                                               .shader_stage()
                                               .compute_shader(shaders[i])
                                               .layout()
                                               .add_descriptor_set_layouts(state_->layouts)
                                               .add_push_constant_range(vkz::shader_stage_compute, 0, sizeof(uint32_t))
                                               .name(names[i])
                                               .build();
        }
    }

    void ddgi::initialize_resources() {
        const auto &input = *state_->input;

        command_pool uploads{device_, input.queue_family, vkz::command_pool_flags_transient, input.queue};
        const auto cmd = uploads.create_command_buffer();

        for (auto *value : {&indirect_light, &radiance, &probe_grid_irradiance, &probe_grid_visibility, &probe_offset, &probe_hit_geometry}) {
            clear(cmd, value->image, VkClearColorValue{{0, 0, 0, 0}}, {});
            barrier::push_and_flush(cmd, value->image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_transfer, shader_stages,
                                    vkz::access_transfer_write, vkz::access_shader_read_write, VK_IMAGE_LAYOUT_GENERAL);
        }

        if (!state_->borrowed_material_textures) {
            auto &fallback = state_->material_textures.front();
            clear(cmd, fallback.image, VkClearColorValue{{1, 1, 1, 1}}, {});
            barrier::push_and_flush(cmd, fallback.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_transfer, shader_stages,
                                    vkz::access_transfer_write, vkz::access_shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        state_->probe_pipeline.sbt.upload(cmd);

        uploads.submit_and_wait(cmd);
    }

    void ddgi::new_frame(const frame_info &frame) {
        state_->frame = frame;
        new_frame();
    }

    void ddgi::new_frame() {
        if (!state_->initialized)
            throw std::logic_error("DDGI is not initialized");
        if (lights_.size() > state_->light_capacity)
            throw std::logic_error("DDGI light list exceeded its initial capacity");

        auto &data = state_->data;
        const auto &config = state_->config;

        data.probe_grid_position = config.grid_position;
        data.probe_spacing = probes_.spacing;
        data.reciprocal_probe_spacing = probes_.inverse_spacing;
        data.probe_counts = probes_.count;
        data.probe_rays = int32_t(probes_.rays_per_probe);

        data.probe_update_offset = probes_.update_offsets % state_->num_probes;
        data.probe_update_count = probes_.update_counts == 0 ? int32_t(state_->num_probes) - data.probe_update_offset : std::min(probes_.update_counts, int32_t(state_->num_probes) - data.probe_update_offset);
        data.irradiance_side_length = int32_t(irradiance_side_length_);
        data.visibility_side_length = int32_t(visibility_side_length_);

        data.hysteresis = config.hysteresis;
        data.self_shadow_bias = config.self_shadow_bias;
        data.infinite_bounces_multiplier = config.infinite_bounces_multiplier;
        data.output_resolution_half = half_resolution_;

        data.num_lights = uint32_t(lights_.size());
        data.ddgi_debug_options = 8 | 16 | 64 | (config.perceptual_encoding ? 32 : 0) | (config.infinite_bounces ? 512 : 0)
            | (config.relocate_probes ? 128 : 0) | (config.classify_probes ? 256 : 0);
        data.max_ray_distance = config.max_ray_distance;
        data.inverse_view_projection = state_->frame.inverse_view_projection;
        data.camera_position = state_->frame.camera_position;
        data.max_offset = probes_.max_offset;
        data.sphere_scale = probes_.sphere_scale;

        const auto angle = float(data.frame_index) * 2.39996323f;
        data.random_rotation = glm::rotate(glm::mat4(1), angle, glm::normalize(glm::vec3(std::sin(angle + 1), std::cos(angle + 2), std::sin(angle + 3))));
    }

    void ddgi::probe_rt(VkCommandBuffer cmd) {
        if (!state_->initialized)
            throw std::logic_error("DDGI is not initialized");
        synchronize(cmd);

        update(cmd, state_->constants_buffer, state_->data);

        auto lights = pack_lights(lights_);
        update(cmd, state_->light_buffer, lights.size() * sizeof(lights.front()), lights.data());
        barrier::push_and_flush(cmd, vkz::pipeline_stage_transfer, shader_stages, vkz::access_transfer_write, vkz::access_uniform_read | vkz::access_shader_read);

        bind_pipeline(cmd, state_->probe_pipeline);
        bind_descriptor_sets(cmd, state_->probe_pipeline, state_->descriptor_sets);

        trace_ray(cmd, state_->probe_pipeline, probes_.rays_per_probe, uint32_t(state_->data.probe_update_count), "probes");
        synchronize(cmd);

    }

    void ddgi::update_irradiance(VkCommandBuffer cmd) {
        if (!state_->initialized)
            throw std::logic_error("DDGI is not initialized");

        const auto &pipeline = state_->compute_pipelines[0];
        bind_pipeline(cmd, pipeline);
        bind_descriptor_sets(cmd, pipeline, state_->descriptor_sets);

        for (uint32_t pass = 0; pass < 2; ++pass) {
            push_constants(cmd, pipeline, pass, vkz::shader_stage_compute);
            dispatch(cmd, (irradiance_atlas_width + 7) / 8, (irradiance_atlas_height + 7) / 8, 1);

            synchronize(cmd);

        }
    }

    void ddgi::update_visibility(VkCommandBuffer cmd) {
        if (!state_->initialized)
            throw std::logic_error("DDGI is not initialized");

        const auto &pipeline = state_->compute_pipelines[1];
        bind_pipeline(cmd, pipeline);
        bind_descriptor_sets(cmd, pipeline, state_->descriptor_sets);

        for (uint32_t pass = 0; pass < 2; ++pass) {
            push_constants(cmd, pipeline, pass, vkz::shader_stage_compute);
            dispatch(cmd, (visibility_atlas_width + 7) / 8, (visibility_atlas_height + 7) / 8, 1);

            synchronize(cmd);

        }
    }

    void ddgi::sample_indirect(VkCommandBuffer cmd) {
        if (!state_->initialized)
            throw std::logic_error("DDGI is not initialized");

        const auto &pipeline = state_->compute_pipelines[2];
        bind_pipeline(cmd, pipeline);
        bind_descriptor_sets(cmd, pipeline, state_->descriptor_sets);
        const auto extent = indirect_light.image.create_info.extent;

        dispatch(cmd, (extent.width + 7) / 8, (extent.height + 7) / 8, 1);
        barrier::push_and_flush(cmd, vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_all_commands, vkz::access_shader_write, vkz::access_shader_read);
    }

    void ddgi::exec(VkCommandBuffer cmd) {
        probe_rt(cmd);
        update_irradiance(cmd);
        update_visibility(cmd);
        sample_indirect(cmd);
        update_probes(cmd);
    }

    void ddgi::update_probes(VkCommandBuffer cmd) {
        if (!state_->config.relocate_probes && !state_->config.classify_probes)
            return;
        synchronize(cmd);

        const auto &pipeline = state_->compute_pipelines[3];
        bind_pipeline(cmd, pipeline);
        bind_descriptor_sets(cmd, pipeline, state_->descriptor_sets);
        dispatch(cmd, (uint32_t(state_->data.probe_update_count) + 63) / 64, 1, 1);
        synchronize(cmd);

    }

    void ddgi::end_frame() {
        if (!state_->initialized)
            throw std::logic_error("DDGI is not initialized");

        ++state_->data.frame_index;
        if (probes_.update_counts > 0) probes_.update_offsets = (probes_.update_offsets + state_->data.probe_update_count) % int32_t(state_->num_probes);
    }

    void ddgi::init_textures() {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device_.physical, &properties);
        const auto dimension_limit = properties.limits.maxImageDimension2D;
        const auto xy = uint64_t(probes_.count.x) * uint64_t(probes_.count.y);
        if ((uint64_t(irradiance_side_length_) + 2) * xy > dimension_limit
            || (uint64_t(visibility_side_length_) + 2) * xy > dimension_limit
            || (uint64_t(irradiance_side_length_) + 2) * uint64_t(probes_.count.z) > dimension_limit
            || (uint64_t(visibility_side_length_) + 2) * uint64_t(probes_.count.z) > dimension_limit
            || xy * uint64_t(probes_.count.z) > dimension_limit || probes_.rays_per_probe > dimension_limit)
            throw std::invalid_argument("DDGI atlas or radiance dimensions exceed device limits");

        const auto create = [&](texture &target, VkFormat format, uint32_t width, uint32_t height) {
            if (width == 0 || height == 0 || width > dimension_limit || height > dimension_limit)
                throw std::invalid_argument("Invalid DDGI texture dimensions");
            target.destroy();
            target = {};
            target.image = image::builder(allocator_)
                .type(VK_IMAGE_TYPE_2D)
                .format(format)
                .extent(width, height)
                .usage(vkz::image_usage_sampled_storage_transfer)
                .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                .build();

            target.image_view =
                image_view::builder(device_)
                    .image(target.image)
                    .view_type(VK_IMAGE_VIEW_TYPE_2D)
                    .format(format)
                    .aspect_mask(vkz::image_aspect_color)
                    .level_count(1)
                    .layer_count(1)
                    .build();

            target.sampler =
                sampler::builder(device_)
                    .mag_filter(VK_FILTER_LINEAR)
                    .min_filter(VK_FILTER_LINEAR)
                    .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                .build();

        };

        const auto divisor = half_resolution_ ? 2u : 1u;
        create(indirect_light, VK_FORMAT_R16G16B16A16_SFLOAT,
            screen_resolution_.x / divisor, screen_resolution_.y / divisor);

        const auto count = glm::uvec3(probes_.count);
        const auto num_probes = count.x * count.y * count.z;
        create(radiance, VK_FORMAT_R16G16B16A16_SFLOAT, probes_.rays_per_probe, num_probes);
        create(probe_hit_geometry, VK_FORMAT_R16G16B16A16_SFLOAT, probes_.rays_per_probe, num_probes);

        const auto octahedral_irradiance_size = irradiance_side_length_ + 2;
        irradiance_atlas_width = octahedral_irradiance_size * count.x * count.y;
        irradiance_atlas_height = octahedral_irradiance_size * count.z;
        create(probe_grid_irradiance, VK_FORMAT_R16G16B16A16_SFLOAT, irradiance_atlas_width, irradiance_atlas_height);

        const auto octahedral_visibility_size = visibility_side_length_ + 2;
        visibility_atlas_width = octahedral_visibility_size * count.x * count.y;
        visibility_atlas_height = octahedral_visibility_size * count.z;
        create(probe_grid_visibility, VK_FORMAT_R16G16_SFLOAT, visibility_atlas_width, visibility_atlas_height);

        create(probe_offset, VK_FORMAT_R16G16B16A16_SFLOAT, count.x * count.y, count.z);
    }

}
