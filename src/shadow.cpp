#include <vulkanizer/shadow.hpp>
#include <vulkanizer/reference.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/descriptor_set_builder.hpp>
#include <vulkanizer/compute_pipeline_builder.hpp>
#include <vulkanizer/detail/compute_pipeline_layout_builder.hpp>
#include <vulkanizer/detail/compute_shader_stage_builder.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "shadow_motion_vector_comp.hpp"
#include "shadow_variance_comp.hpp"
#include "shadow_max_variation_comp.hpp"
#include "shadow_filter_variation_comp.hpp"
#include "shadow_visibility_comp.hpp"
#include "shadow_filter_visibility_comp.hpp"
#include "shadow_raw_visibility_comp.hpp"

namespace vkz {
    namespace {
        enum texture_index : size_t {
            motion_vector,
            visibility_cache,
            variation,
            max_variation,
            filtered_variation,
            filtered_visibility,
            sample_count_cache,
            alternate_visibility_cache,
            world_history_0,
            world_history_1,
            normal_history_0,
            normal_history_1,
            penumbra,
            texture_count
        };

        struct light_uniform {
            glm::vec4 position_range;
            glm::vec4 direction_type;
            glm::vec4 radiance;
            glm::vec4 cone;
        };

        static_assert(sizeof(light_uniform) == 64);

        struct constants {
            glm::mat4 inverse_view_projection{1};
            glm::mat4 previous_view_projection{1};
            glm::vec3 camera_position{};
            uint32_t frame_index{};
            glm::uvec2 resolution{};
            uint32_t num_lights{};
            uint32_t history_count{};
            uint32_t vertex_stride{};
            uint32_t alpha_test{};
            uint32_t world_positions{};
            float directional_distance{100};
        };

        static_assert(sizeof(constants) == 176);

        template <size_t N> std::vector<uint32_t> shader_words(const uint32_t (&words)[N]) {
            return {words, words + N};
        }

        void synchronize(VkCommandBuffer cmd) {
            barrier::push_and_flush(cmd, vkz::pipeline_stage_all_commands, vkz::pipeline_stage_compute_shader | vkz::pipeline_stage_transfer, vkz::access_memory_read_write,
                                    vkz::access_shader_read | vkz::access_shader_write | vkz::access_transfer_write);
        }
    } // namespace

    struct shadow::implementation {
        scene_input input;
        frame_info frame;
        constants data;
        buffer constants_buffer, light_buffer, instances_buffer, materials_buffer;
        sampler input_sampler;
        descriptor_set_layout layout, surface_layout;
        descriptor_pool pool;
        descriptor_set set, surface_set;
        std::array<pipeline, 7> pipelines{};
        std::array<texture, texture_count> textures{};
        std::vector<light_uniform> previous_lights;
        bool initialized{};
    };

    shadow::shadow(vkz::device device, vma_memory_allocator &allocator, std::span<const mesh::light> lights, const texture &depth_buffer, const texture &normal_buffer, glm::uvec2 screen_resolution,
                   bool half_resolution)
        : state_(std::make_unique<implementation>()), device_(device), allocator_(allocator), lights_(lights), depth_buffer_(depth_buffer), normal_buffer_(normal_buffer),
          screen_resolution_(screen_resolution), half_resolution_(half_resolution) {
    }

    shadow::~shadow() {
        if (state_->input.queue) batch_submission{state_->input.queue}.wait_idle();

        for (auto &value : state_->pipelines)
            value.destroy();

        state_->pool.destroy();
        state_->layout.destroy();
        state_->surface_layout.destroy();
        state_->instances_buffer.destroy();
        state_->materials_buffer.destroy();
        state_->constants_buffer.destroy();
        state_->light_buffer.destroy();
        state_->input_sampler.destroy();

        for (auto &value : state_->textures)
            value.destroy();
    }

    void shadow::init(const scene_input &scene) {
        if (state_->initialized) throw std::logic_error("Shadow is already initialized");

        if (!device_ || !scene.tlas || !scene.queue || !screen_resolution_.x || !screen_resolution_.y) throw std::invalid_argument("Shadow requires a TLAS and a nonzero resolution");

        for (const auto *value : {&depth_buffer_, &normal_buffer_}) {
            const auto extent = value->image.create_info.extent;

            if (!value->image_view.handle || extent.width != screen_resolution_.x || extent.height != screen_resolution_.y ||
                !(value->image.create_info.usage & static_cast<VkImageUsageFlags>(vkz::image_usage_sampled)))
                throw std::invalid_argument("Shadow requires sampled G-buffer inputs at screen resolution");
        }

        if (scene.scene && (scene.material_textures.empty() || !scene.geometry.vertices || !scene.geometry.indices))
            throw std::invalid_argument("Alpha-aware shadows require geometry and material textures");
        state_->input = scene;
        state_->data.alpha_test = scene.scene != nullptr;
        if (scene.scene) state_->data.vertex_stride = reference::vertex_stride(*scene.scene);
        state_->data.resolution = glm::max((screen_resolution_ + (half_resolution_ ? 1u : 0u)) / (half_resolution_ ? 2u : 1u), glm::uvec2(1));
        state_->data.num_lights = uint32_t(lights_.size());

        create_buffers();
        init_textures();
        create_descriptor_set_layouts();
        create_descriptor_pool();
        update_descriptors();
        create_pipelines();
        initialize_resources();

        state_->initialized = true;
    }

    void shadow::create_buffers() {
        state_->constants_buffer = buffer::builder(allocator_)
            .size(sizeof(constants))
            .usage(vkz::buffer_usage_uniform_upload)
            .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
            .build();

        state_->light_buffer =
            buffer::builder(allocator_)
                .size(std::max(size_t(1), lights_.size()) * sizeof(light_uniform))
                .usage(vkz::buffer_usage_storage_upload)
                .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                .build();

        const auto upload = [&](const void *data, size_t size) {
            auto value = buffer::builder(allocator_)
                .size(size)
                .usage(vkz::buffer_usage_storage)
                .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .build();
            auto mapping = value.map();
            std::memcpy(mapping.as<void>(), data, size);
            value.flush();
            mapping.unmap();
            return value;
        };

        std::vector<reference::ray_tracing_instance_uniform> instances(1);
        std::vector<reference::material_uniform> materials(1);
        if (state_->input.scene) {
            instances = reference::make_ray_tracing_instances(*state_->input.scene);
            materials.clear();
            for (const auto &material : state_->input.scene->materials)
                materials.push_back(reference::make_material_uniform(*state_->input.scene, material));
        }
        state_->instances_buffer = upload(instances.data(), instances.size() * sizeof(instances.front()));
        state_->materials_buffer = upload(materials.data(), materials.size() * sizeof(materials.front()));
    }

    void shadow::init_textures() {
        const std::array formats{VK_FORMAT_R16G16_SFLOAT,       VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16_SFLOAT,          VK_FORMAT_R16_SFLOAT,          VK_FORMAT_R16_SFLOAT,
                                 VK_FORMAT_R16_SFLOAT,          VK_FORMAT_R8G8B8A8_UINT,       VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
                                 VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16_SFLOAT};

        for (size_t i = 0; i < state_->textures.size(); ++i) {
            auto &value = state_->textures[i];
            const bool volume = (i != motion_vector && i < world_history_0) || i == penumbra;
            value.image = image::builder(allocator_)
                .type(volume ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D)
                .format(formats[i])
                .extent(state_->data.resolution.x, state_->data.resolution.y, volume ? std::max(1u, state_->data.num_lights) : 1)
                .usage(vkz::image_usage_sampled_storage_transfer)
                .build();

            value.image_view = image_view::builder(device_)
                .image(value.image)
                .view_type(volume ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D)
                .format(formats[i])
                .aspect_mask(vkz::image_aspect_color)
                .level_count(1)
                .layer_count(1)
                .build();

            value.sampler =
                sampler::builder(device_)
                    .mag_filter(i == sample_count_cache ? VK_FILTER_NEAREST : VK_FILTER_LINEAR)
                    .min_filter(i == sample_count_cache ? VK_FILTER_NEAREST : VK_FILTER_LINEAR)
                    .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                .build();
        }

        state_->input_sampler =
            sampler::builder(device_)
                .mag_filter(VK_FILTER_NEAREST)
                .min_filter(VK_FILTER_NEAREST)
                .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
            .build();
    }

    void shadow::create_descriptor_set_layouts() {
        state_->layout = descriptor_set_layout_builder{device_}
                             .binding(0)
                             .descriptor_type(vkz::descriptor_type_uniform_buffer)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(1)
                             .descriptor_type(vkz::descriptor_type_storage_buffer)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(2)
                             .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(3)
                             .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(4)
                             .descriptor_type(vkz::descriptor_type_acceleration_structure)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(5)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(6)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(7)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(8)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(9)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(10)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(11)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(12)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(13)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(14)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(15)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(16)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(17)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(18)
                             .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .create_layout();

        state_->surface_layout = descriptor_set_layout_builder{device_}
                                     .binding(0)
                                     .descriptor_type(vkz::descriptor_type_storage_buffer)
                                     .descriptor_count(1)
                                     .shader_stages(vkz::shader_stage_compute)
                                     .binding(1)
                                     .descriptor_type(vkz::descriptor_type_storage_buffer)
                                     .descriptor_count(1)
                                     .shader_stages(vkz::shader_stage_compute)
                                     .binding(2)
                                     .descriptor_type(vkz::descriptor_type_storage_buffer)
                                     .descriptor_count(1)
                                     .shader_stages(vkz::shader_stage_compute)
                                     .binding(3)
                                     .descriptor_type(vkz::descriptor_type_storage_buffer)
                                     .descriptor_count(1)
                                     .shader_stages(vkz::shader_stage_compute)
                                     .binding(5)
                                     .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                     .descriptor_count(std::max(1u, uint32_t(state_->input.material_textures.size())))
                                     .shader_stages(vkz::shader_stage_compute)
                                     .create_layout();
    }

    void shadow::create_descriptor_pool() {
        state_->pool = descriptor_pool{device_,
                                       2,
                                       {{static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 1},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 5},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 3 + std::max(1u, uint32_t(state_->input.material_textures.size()))},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_acceleration_structure), 1},
                                        {static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), uint32_t(texture_count)}}};

        state_->set = state_->pool.allocate(state_->layout);
        state_->surface_set = state_->pool.allocate(state_->surface_layout);
    }

    void shadow::update_descriptors() {
        descriptor_bindings bindings{.descriptor_set = state_->set,
                                     .bindings = {descriptor<ubo_descriptor>(state_->constants_buffer, 0), descriptor<buffer_descriptor>(state_->light_buffer, 1),
                                                  texture_descriptor{depth_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2},
                                                  texture_descriptor{normal_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3}}};

        for (uint32_t i = 0; i < state_->textures.size(); ++i)
            bindings.bindings.push_back(image_descriptor{state_->textures[i].image_view, VK_IMAGE_LAYOUT_GENERAL, i + 5});

        bindings.bindings.push_back(texture_descriptor{depth_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 18});
        update_descriptor(device_, std::move(bindings));

        texture_array_descriptor textures;
        textures.binding = 5;
        for (const auto &value : state_->input.material_textures)
            textures.push_back(descriptor<texture_descriptor>(value, 5));
        if (state_->input.material_textures.empty()) textures.push_back(texture_descriptor{depth_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 5});

        const auto &geometry = state_->input.geometry;
        update_descriptor(device_, {.descriptor_set = state_->surface_set,
                                    .bindings = {descriptor<buffer_descriptor>(state_->input.scene ? geometry.vertices : state_->materials_buffer, 0),
                                                 descriptor<buffer_descriptor>(state_->input.scene ? geometry.indices : state_->materials_buffer, 1),
                                                 descriptor<buffer_descriptor>(state_->instances_buffer, 2), descriptor<buffer_descriptor>(state_->materials_buffer, 3), textures}});

        // descriptor_t currently has no acceleration-structure binding helper.
        VkWriteDescriptorSetAccelerationStructureKHR acceleration{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        acceleration.accelerationStructureCount = 1;
        acceleration.pAccelerationStructures = &state_->input.tlas;

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.pNext = &acceleration;
        write.dstSet = state_->set.handle;
        write.dstBinding = 4;
        write.descriptorType = static_cast<VkDescriptorType>(vkz::descriptor_type_acceleration_structure);
        write.descriptorCount = 1;

        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }

    void shadow::create_pipelines() {
        const std::array shaders{shader_words(shadow_motion_vector_comp),    shader_words(shadow_variance_comp),   shader_words(shadow_max_variation_comp),
                                 shader_words(shadow_filter_variation_comp), shader_words(shadow_visibility_comp), shader_words(shadow_filter_visibility_comp),
                                 shader_words(shadow_raw_visibility_comp)};

        for (size_t i = 0; i < shaders.size(); ++i) {
            state_->pipelines[i] =
                compute_pipeline_builder{device_}
                    .shader_stage()
                        .compute_shader(shaders[i])
                            .add_specialization(std::max(1u, uint32_t(state_->input.material_textures.size())), 0)
                    .layout()
                        .add_descriptor_set_layout(state_->layout)
                        .add_descriptor_set_layout(state_->surface_layout)
                .build();
        }
    }

    void shadow::initialize_resources() {
        command_pool uploads{device_, state_->input.queue_family, vkz::command_pool_flags_transient, state_->input.queue};
        const auto cmd = uploads.create_command_buffer();

        for (auto &value : state_->textures) {
            clear(cmd, value.image, VkClearColorValue{}, {});
            barrier::push_and_flush(cmd, value.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_transfer, vkz::pipeline_stage_compute_shader,
                                    vkz::access_transfer_write, vkz::access_shader_read_write, VK_IMAGE_LAYOUT_GENERAL);
        }

        uploads.submit_and_wait(cmd);
    }

    void shadow::new_frame(const frame_info &frame) {
        state_->frame = frame;
        state_->data.inverse_view_projection = frame.inverse_view_projection;
        state_->data.camera_position = frame.camera_position;
        state_->data.world_positions = frame.world_positions.handle != VK_NULL_HANDLE;
        state_->data.directional_distance = frame.directional_distance;
        update_descriptor(
            device_, {.descriptor_set = state_->set,
                      .bindings = {texture_descriptor{frame.world_positions.handle ? frame.world_positions : depth_buffer_.image_view, state_->input_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 18}}});
    }

    void shadow::exec(VkCommandBuffer cmd) {
        if (!state_->initialized) throw std::logic_error("Shadow is not initialized");

        synchronize(cmd);

        std::vector<light_uniform> lights;

        for (const auto &light : lights_) {
            lights.push_back({glm::vec4(light.position, light.range),
                              glm::vec4(light.direction, float(light.type)),
                              glm::vec4(light.color * light.intensity, 0),
                              {std::cos(light.inner_cone_angle), std::cos(light.outer_cone_angle), light.source_radius, 0}});
        }

        if (lights.size() != state_->previous_lights.size() || (!lights.empty() && std::memcmp(lights.data(), state_->previous_lights.data(), lights.size() * sizeof(lights.front())) != 0)) {
            state_->data.history_count = 0;
            state_->previous_lights = lights;
        }

        update(cmd, state_->constants_buffer, state_->data);

        if (!lights.empty()) update(cmd, state_->light_buffer, lights.size() * sizeof(lights.front()), lights.data());

        barrier::push_and_flush(cmd, vkz::pipeline_stage_transfer, vkz::pipeline_stage_compute_shader, vkz::access_transfer_write, vkz::access_uniform_read | vkz::access_shader_read);

        const size_t first = state_->frame.denoise ? 0 : 6;
        const size_t last = state_->frame.denoise ? 6 : 7;
        for (size_t i = first; i < last; ++i) {
            const auto &pipeline = state_->pipelines[i];
            bind_pipeline(cmd, pipeline);
            bind_descriptor_sets(cmd, pipeline, {state_->set, state_->surface_set});
            dispatch(cmd, (state_->data.resolution.x + 7) / 8, (state_->data.resolution.y + 7) / 8, i == 0 ? 1 : std::max(1u, state_->data.num_lights));

            barrier::push_and_flush(cmd, vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_all_commands, vkz::access_shader_read_write, vkz::access_shader_read_write);
        }
    }

    void shadow::end_frame() {
        state_->data.previous_view_projection = state_->frame.view_projection;
        ++state_->data.frame_index;
        state_->data.history_count = state_->frame.denoise ? std::min(state_->data.history_count + 1, 4u) : 0;
    }

    const texture &shadow::penumbra_texture() const {
        return state_->textures[penumbra];
    }

    const texture &shadow::visibility_texture() const {
        return state_->textures[filtered_visibility];
    }
} // namespace vkz
