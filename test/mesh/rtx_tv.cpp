#include "rtx_tv.hpp"
#include "rtx_ltc_tables.hpp"

#include <vulkanizer/commands.hpp>
#include <vulkanizer/descriptor_set_builder.hpp>
#include <vulkanizer/compute_pipeline_builder.hpp>
#include <vulkanizer/detail/compute_shader_stage_builder.hpp>
#include <vulkanizer/detail/compute_pipeline_layout_builder.hpp>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <glm/gtc/packing.hpp>
#include <iostream>

namespace rtx {
    tv_light::tv_light(vkz::vma_memory_allocator &allocator, VkQueue queue, uint32_t queue_family,
        const vkz::path &video_path, std::span<const uint32_t> filter_shader)
        : allocator_(allocator), device_(allocator.device), queue_(queue), queue_family_(queue_family) {
        const auto clip = vkz::video::clip::load(video_path);
        const auto decode_family = vkz::video::decode_queue_family(device_.physical);
        decoder_ = std::make_unique<vkz::video::decoder>(vkz::video::decoder_params{
            &allocator_, queue_, queue_family_, vkz::get_queue(device_, decode_family), decode_family}, clip);

        screen_ = create_texture(clip.width(), clip.height());
        filtered_ = create_texture(256, 256, 11);
        ltc_matrix_ = load_table(ltc_1);
        ltc_amplitude_ = load_table(ltc_2);

        parameters_ = vkz::buffer::builder(allocator_)
            .size(sizeof(data_))
            .usage(vkz::buffer_usage_uniform)
            .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .build();

        filter_layout_ = vkz::descriptor_set_layout_builder{device_}
                             .binding(0)
                             .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(1)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .binding(2)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .create_layout();

        filter_pool_ = vkz::descriptor_pool{
            device_, 1, {{static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 1}, {static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), 2}}};
        filter_set_ = filter_pool_.allocate(filter_layout_);

        filter_pipeline_ = vkz::compute_pipeline_builder{device_}
                               .shader_stage()
                               .compute_shader(std::vector<uint32_t>(filter_shader.begin(), filter_shader.end()))
                               .layout()
                               .add_descriptor_set_layout(filter_layout_)
                               .add_push_constant_range(vkz::shader_stage_compute, 0, sizeof(int))
                               .build();

        const auto frames = clip.frames();
        const auto shortest = std::min_element(frames.begin(), frames.end(), [](const auto &a, const auto &b) {
            return a.duration < b.duration;
        });
        sync_window_ = shortest->duration * 0.5;

        new_frame();
        update_strength(radiance_scale);

        if (auto pcm = vkz::audio::load_media(video_path)) {
            audio_ = std::make_unique<vkz::audio::engine>();
            audio_->load(std::move(*pcm));
        }

        std::cout << "TV audio: " << (audio_ ? "track loaded" : "no audio track") << '\n';
    }

    tv_light::~tv_light() {
        // Stop the callback before releasing the scene's playback resources.
        audio_.reset();

        filter_pipeline_.destroy();
        filter_pool_.destroy();
        filter_layout_.destroy();
        parameters_.destroy();
        ltc_amplitude_.destroy();
        ltc_matrix_.destroy();
        filtered_.destroy();
        screen_.destroy();
    }

    vkz::texture tv_light::create_texture(uint32_t width, uint32_t height, uint32_t layers) {
        vkz::texture result;
        result.image = vkz::image::builder(allocator_)
            .format(VK_FORMAT_R16G16B16A16_SFLOAT)
            .extent(width, height)
            .array_layers(layers)
            .usage(vkz::image_usage_sampled_storage_transfer)
            .build();

        result.image_view = vkz::image_view::builder(device_)
                                .image(result.image)
                                .format(VK_FORMAT_R16G16B16A16_SFLOAT)
                                .view_type(layers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY)
                                .subresource_range({static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, layers})
                                .build();

        result.sampler =
            vkz::sampler::builder(device_)
                .mag_filter(VK_FILTER_LINEAR)
                .min_filter(VK_FILTER_LINEAR)
                .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
                .border_color(VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK)
            .build();

        return result;
    }

    vkz::texture tv_light::load_table(std::span<const float> values) {
        auto result = create_texture(64, 64);
        std::vector<uint16_t> half_values(values.size());

        for (size_t i = 0; i < values.size(); ++i)
            half_values[i] = glm::packHalf1x16(values[i]);

        auto staging = vkz::buffer::builder(allocator_)
            .size(half_values.size() * sizeof(uint16_t))
            .usage(vkz::buffer_usage_transfer_src)
            .memory_usage(VMA_MEMORY_USAGE_CPU_ONLY)
            .build();

        auto mapping = staging.map();
        std::memcpy(mapping.as<void>(), half_values.data(), half_values.size() * sizeof(uint16_t));
        staging.flush();
        mapping.unmap();

        vkz::command_pool uploads{device_, queue_family_, vkz::command_pool_flags_transient, queue_};
        const auto cmd = uploads.create_command_buffer();
        vkz::copy(cmd, staging, result.image);
        vkz::barrier::push_and_flush(cmd, result.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_transfer, vkz::pipeline_stage_all_commands,
                                     vkz::access_transfer_write, vkz::access_shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        uploads.submit_and_wait(cmd);
        staging.destroy();
        return result;
    }

    void tv_light::configure(vkz::mesh::data &scene) {
        // LivingRoom's television screen is a distinct four-vertex mesh/material.
        const auto &placement = scene.instances.at(174);
        const auto &mesh = scene.meshes.at(placement.mesh_id);
        if (mesh.vertex_count != 4 || mesh.material_id != 33)
            throw std::runtime_error("Living room TV screen geometry changed");

        const auto stride = scene.streams.bindings.front().stride;

        // UV (0,0), (1,0), (1,1), (0,1), front-facing winding after transformation.
        const std::array<uint32_t, 4> order{1, 0, 3, 2};

        for (size_t i = 0; i < order.size(); ++i) {
            glm::vec3 position;
            std::memcpy(&position, scene.vertex_data.data() + (mesh.vertex_offset + order[i]) * stride, sizeof(position));
            data_.points[i] = placement.model * glm::vec4(position, 1);
        }

        auto &material = scene.materials.at(33);
        material.base_color_factor = glm::vec4(1);
        material.emissive_factor = glm::vec3(radiance_scale);
        material.metallic_factor = 0;
        material.emissive_texture.index = int(scene.textures.size());
        material.emissive_texture.scale = {1 / (0.987805f - 0.0121951f), 1 / (0.98790318f - 0.262097f)};
        material.emissive_texture.offset = -glm::vec2(0.0121951f, 0.262097f) * material.emissive_texture.scale;
        scene.textures.push_back({}); // RGB output replaces this slot during texture upload.

        data_.settings.x = 1;
        update_strength(radiance_scale);
    }

    void tv_light::update_strength(float value) {
        data_.settings.y = value;
        auto mapping = parameters_.map();
        std::memcpy(mapping.as<void>(), &data_, sizeof(data_));
        parameters_.flush();
        mapping.unmap();
    }

    void tv_light::new_frame() {
        const auto now = std::chrono::steady_clock::now();
        const auto dt = clock_started_ ? std::chrono::duration<double>(now - tick_).count() : 0.0;
        tick_ = now;
        auto next = playback_time_ + dt;

        if (audio_ && clock_started_ && !resume_audio_) {
            // Match video_test: interpolate callback blocks with a 100 ms phase correction.
            const auto sample_clock = audio_->position();
            next += (sample_clock - next) * -std::expm1(-dt / 0.1);
            next = std::clamp(next, sample_clock - sync_window_, sample_clock + sync_window_);
        }

        playback_time_ = std::max(playback_time_, next);
        const auto duration = decoder_->source().duration();

        if (playback_time_ >= duration) {
            playback_time_ = std::fmod(playback_time_, duration);
            ++playback_loops_;
            last_frame_ = ~0u;

            if (audio_) {
                audio_->pause(true);
                audio_->seek(playback_time_);
                resume_audio_ = true;
            }
        }

        if (audio_ && clock_started_ && !resume_audio_)
            max_av_drift_ = std::max(max_av_drift_, std::abs(audio_->position() - playback_time_));

        const auto frame_index = decoder_->source().frame_at(playback_time_);

        if (frame_index == last_frame_)
            return;

        // Decoder outputs are borrowed; finish both decoding and filtering before releasing one.
        vkz::batch_submission{queue_}.wait_idle();
        filter_frame(decoder_->frame(frame_index));
        last_frame_ = frame_index;
        ++decoded_frames_;
    }

    void tv_light::frame_submitted() {
        // Begin/resume sound after the initial or looped video frame has been submitted.
        if (!clock_started_ || resume_audio_) {
            tick_ = std::chrono::steady_clock::now();
            clock_started_ = true;
            resume_audio_ = false;

            if (audio_)
                audio_->pause(false);
        }
    }

    void tv_light::verify() {
        vkz::batch_submission{queue_}.wait_idle();

        if (audio_) {
            std::cout << "TV audio synchronization: max drift=" << max_av_drift_
                << " s, loops=" << playback_loops_ << '\n';

            if (audio_->position() <= 0 || max_av_drift_ > sync_window_ * 2)
                throw std::runtime_error("TV audio clock stalled or drifted by more than one video frame");
        }

        for (auto *target : {&screen_, &filtered_}) {
            const auto extent = target->image.create_info.extent;
            const auto count = size_t(extent.width) * extent.height * target->image.create_info.arrayLayers;
            auto readback = vkz::buffer::builder(allocator_)
                .size(count * 4 * sizeof(uint16_t))
                .usage(vkz::buffer_usage_transfer_dst)
                .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
                .build();

            vkz::command_pool commands{device_, queue_family_, vkz::command_pool_flags_transient, queue_};
            const auto cmd = commands.create_command_buffer();
            vkz::copy(cmd, *target, readback);
            vkz::barrier::push_and_flush(cmd, target->image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, target->image.create_info.arrayLayers},
                                         vkz::pipeline_stage_transfer, vkz::pipeline_stage_all_commands, vkz::access_transfer_read, vkz::access_shader_read,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            vkz::barrier::gpu_to_cpu(cmd);
            commands.submit_and_wait(cmd);

            readback.invalidate();
            auto mapping = readback.map();
            const auto *values = mapping.as<uint16_t>();
            double sum = 0;
            double chroma = 0;
            bool valid = true;

            for (size_t i = 0; i < count; ++i) {
                const glm::vec3 rgb{glm::unpackHalf1x16(values[4 * i]),
                    glm::unpackHalf1x16(values[4 * i + 1]), glm::unpackHalf1x16(values[4 * i + 2])};

                for (int c = 0; c < 3; ++c)
                    valid &= std::isfinite(rgb[c]) && rgb[c] >= 0;

                sum += rgb.x + rgb.y + rgb.z;
                chroma += std::abs(rgb.x - rgb.z);
            }

            mapping.unmap();
            readback.destroy();
            std::cout << (target == &screen_ ? "TV video" : "TV LTC filtering")
                << ": mean=" << sum / (count * 3) << ", chroma=" << chroma / count
                << ", decoded frames=" << decoded_frames_ << '\n';

            if (!valid || sum <= 0 || chroma <= 0 || decoded_frames_ < 2)
                throw std::runtime_error("TV playback/filtering produced invalid, empty, or static radiance");
        }
    }

    void tv_light::filter_frame(const vkz::texture &frame) {
        vkz::update_descriptor(device_, {.descriptor_set = filter_set_,
                                         .bindings = {vkz::descriptor<vkz::texture_descriptor>(frame, 0), vkz::image_descriptor{screen_.image_view, VK_IMAGE_LAYOUT_GENERAL, 1},
                                                      vkz::image_descriptor{filtered_.image_view, VK_IMAGE_LAYOUT_GENERAL, 2}}});

        vkz::command_pool commands{device_, queue_family_, vkz::command_pool_flags_transient, queue_};
        const auto cmd = commands.create_command_buffer();

        for (auto *target : {&screen_, &filtered_}) {
            vkz::barrier::push_and_flush(cmd, target->image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, target->image.create_info.arrayLayers},
                                         vkz::pipeline_stage_all_commands, vkz::pipeline_stage_compute_shader, vkz::access_shader_read, vkz::access_shader_write, VK_IMAGE_LAYOUT_GENERAL);
        }

        vkz::bind_pipeline(cmd, filter_pipeline_);
        vkz::bind_descriptor_sets(cmd, filter_pipeline_, {filter_set_});
        vkz::push_constants(cmd, filter_pipeline_, 0, vkz::shader_stage_compute);
        vkz::dispatch(cmd, (screen_.image.create_info.extent.width + 7) / 8,
            (screen_.image.create_info.extent.height + 7) / 8, 1);

        for (auto *target : {&screen_, &filtered_}) {
            vkz::barrier::push_and_flush(cmd, target->image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, target->image.create_info.arrayLayers},
                                         vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_compute_shader, vkz::access_shader_write, vkz::access_shader_write, VK_IMAGE_LAYOUT_GENERAL);
        }

        vkz::push_constants(cmd, filter_pipeline_, 1, vkz::shader_stage_compute);
        vkz::dispatch(cmd, 32, 32, 11);

        for (auto *target : {&screen_, &filtered_}) {
            vkz::barrier::push_and_flush(cmd, target->image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, target->image.create_info.arrayLayers},
                                         vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_all_commands, vkz::access_shader_write, vkz::access_shader_read,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        commands.submit_and_wait(cmd);
    }

    void tv_light::verify_lighting(vkz::texture &color, const std::function<void(VkCommandBuffer)> &render) {
        vkz::batch_submission{queue_}.wait_idle();
        const auto extent = color.image.create_info.extent;
        const auto count = size_t(extent.width) * extent.height * 4;
        auto readback = vkz::buffer::builder(allocator_)
            .size(count * sizeof(uint16_t))
            .usage(vkz::buffer_usage_transfer_dst)
            .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
            .build();

        const auto capture = [&] {
            vkz::command_pool commands{device_, queue_family_, vkz::command_pool_flags_transient, queue_};
            const auto cmd = commands.create_command_buffer();
            vkz::copy(cmd, color, readback);
            vkz::barrier::push_and_flush(cmd, color.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_transfer, vkz::pipeline_stage_all_commands,
                                         vkz::access_transfer_read, vkz::access_shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            vkz::barrier::gpu_to_cpu(cmd);
            commands.submit_and_wait(cmd);
            readback.invalidate();
            auto mapping = readback.map();
            const auto *values = mapping.as<uint16_t>();
            std::vector<float> result(count);

            for (size_t i = 0; i < count; ++i)
                result[i] = glm::unpackHalf1x16(values[i]);

            mapping.unmap();
            return result;
        };

        const auto lit = capture();
        update_strength(0);
        {
            vkz::command_pool commands{device_, queue_family_, vkz::command_pool_flags_transient, queue_};
            const auto cmd = commands.create_command_buffer();
            render(cmd);
            commands.submit_and_wait(cmd);
        }

        const auto unlit = capture();
        update_strength(radiance_scale);
        readback.destroy();
        double difference = 0;
        size_t affected_pixels = 0;

        for (size_t i = 0; i < count; i += 4) {
            float contribution = 0;

            for (size_t c = 0; c < 3; ++c) {
                if (!std::isfinite(lit[i + c]) || !std::isfinite(unlit[i + c]))
                    throw std::runtime_error("TV LTC shading produced non-finite scene color");

                contribution += std::max(0.0f, lit[i + c] - unlit[i + c]);
            }

            difference += contribution;
            affected_pixels += contribution > 1e-5;
        }

        std::cout << "TV LTC direct contribution: mean=" << difference / (count * 0.75)
            << ", affected pixels=" << affected_pixels << '\n';

        if (difference <= 0 || affected_pixels == 0)
            throw std::runtime_error("TV LTC light made no contribution to the scene");
    }
}
