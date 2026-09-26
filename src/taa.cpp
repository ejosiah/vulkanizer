#include "vulkanizer/taa.hpp"
#include "vulkanizer/builders.hpp"
#include "vulkanizer/commands.hpp"
#include "vulkanizer/barrier.hpp"
#include "camera_motion.hpp"
#include "resolve.hpp"
#include "resolve16.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace vkz::taa {
    namespace {
        constexpr VkImageSubresourceRange color_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        struct constants {
            glm::mat4 current_view_projection{1};
            glm::mat4 inverse_current_view_projection{1};
            glm::mat4 previous_view_projection{1};
            glm::vec2 jitter_xy{};
            glm::vec2 resolution{};
            glm::uvec4 filters{};
            glm::uvec4 state{};
            glm::vec4 tuning{};
        };

        static_assert(sizeof(constants) == 256);

        void validate(const settings &value) {
            if (uint32_t(value.history_filter) > 1 || uint32_t(value.sample_filter) > 3 || uint32_t(value.constraint) > 4 || !std::isfinite(value.history_weight) || value.history_weight < 0 ||
                value.history_weight >= 1 || !std::isfinite(value.depth_threshold) || value.depth_threshold < 0) {
                throw std::invalid_argument("Invalid TAA settings");
            }
        }

        void validate(const texture &color, const texture &depth) {
            const auto &c = color.image.create_info;
            const auto &d = depth.image.create_info;
            if (!color.image.handle || !color.image_view.handle || !depth.image.handle || !depth.image_view.handle || !c.extent.width || !c.extent.height || c.extent.width != d.extent.width ||
                c.extent.height != d.extent.height || c.imageType != VK_IMAGE_TYPE_2D || d.imageType != VK_IMAGE_TYPE_2D || c.samples != VK_SAMPLE_COUNT_1_BIT || d.samples != VK_SAMPLE_COUNT_1_BIT ||
                (c.format != VK_FORMAT_R16G16B16A16_SFLOAT && c.format != VK_FORMAT_R32G32B32A32_SFLOAT) || !(c.usage & VK_IMAGE_USAGE_SAMPLED_BIT) || !(c.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
                !(d.usage & VK_IMAGE_USAGE_SAMPLED_BIT) || color.image.layout == VK_IMAGE_LAYOUT_UNDEFINED || depth.image.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
                throw std::invalid_argument("TAA requires matching sampled 2D color/depth inputs and a transferable RGBA16F/RGBA32F color image in readable layouts");
            }
        }

        class impl {
          public:
            explicit impl(const params &value) : params_{value} {
                validate(value.color, value.depth);
                validate(value.options);
                if (!value.in_flight_frames || value.depth_channel > 3) {
                    throw std::invalid_argument("TAA requires at least one frame slot and a depth channel in [0,3]");
                }
            }

            ~impl() {
                motion_pipeline_.destroy();
                resolve_pipeline_.destroy();
                pool_.destroy();
                layout_.destroy();
                for (auto &slot : slots_) {
                    slot.uniform.destroy();
                }
                destroy_targets();
                linear_sampler_.destroy();
                point_sampler_.destroy();
            }

            void init() {
                linear_sampler_ = sampler::builder(params_.device)
                    .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                    .mag_filter(VK_FILTER_LINEAR)
                    .min_filter(VK_FILTER_LINEAR)
                    .build();
                point_sampler_ = sampler::builder(params_.device)
                                     .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                                     .mag_filter(VK_FILTER_NEAREST)
                                     .min_filter(VK_FILTER_NEAREST)
                                     .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
                                     .build();
                auto builder = make_descriptor_set_layout_builder(params_.device);
                for (uint32_t binding = 0; binding < 9; ++binding) {
                    const auto type = binding == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                                   : (binding == 5 || binding == 6 || binding == 8 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                    const auto entry = builder.binding(binding)
                        .descriptor_type(type)
                        .descriptor_count(1)
                        .shader_stages(VK_SHADER_STAGE_COMPUTE_BIT);
                    if (binding == 8) {
                        layout_ = entry.create_layout();
                    } else {
                        entry.build();
                    }
                }
                const auto count = params_.in_flight_frames * 2;
                pool_ = descriptor_pool{params_.device,
                                        count,
                                        {
                                            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count},
                                            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count * 5},
                                            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, count * 3},
                                        }};
                slots_.resize(params_.in_flight_frames);
                for (auto &slot : slots_) {
                    slot.uniform = buffer::builder(params_.memory_allocator)
                                       .size(sizeof(constants))
                                       .usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                                       .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                                       .build();
                    for (auto &set : slot.sets) {
                        set = pool_.allocate(layout_);
                    }
                }
                create_targets();
                update_descriptors();
                motion_pipeline_ = make_pipeline(std::vector<uint32_t>{std::begin(taa_camera_motion), std::end(taa_camera_motion)});
                create_resolve_pipeline();
            }

            void resize(const texture &color, const texture &depth) {
                validate(color, depth);
                const auto old_format = params_.color.image.create_info.format;
                params_.color = color;
                params_.depth = depth;
                destroy_targets();
                create_targets();
                update_descriptors();
                if (old_format != color.image.create_info.format) {
                    resolve_pipeline_.destroy();
                    resolve_pipeline_ = {};
                    create_resolve_pipeline();
                }
                reset();
            }

            void configure(const settings &options) {
                validate(options);
                if (!(params_.options == options)) {
                    params_.options = options;
                    reset();
                }
            }

            void reset() {
                history_valid_ = false;
            }

            void update(const camera &value, uint32_t current_frame) {
                if (!std::isfinite(glm::determinant(value.view_projection)) || std::abs(glm::determinant(value.view_projection)) < 1e-12f || !std::isfinite(value.jitter.x) ||
                    !std::isfinite(value.jitter.y)) {
                    throw std::invalid_argument("TAA camera must have an invertible finite view-projection matrix and finite jitter");
                }
                current_camera_ = value;
                current_slot_ = current_frame % slots_.size();
                updated_ = true;
            }

            void resolve(VkCommandBuffer cmd) {
                if (!updated_) {
                    throw std::logic_error("Call taa::update before each resolve");
                }
                auto &slot = slots_[current_slot_];
                const auto &options = params_.options;
                constants data{};
                data.current_view_projection = current_camera_.view_projection;
                data.inverse_current_view_projection = glm::inverse(current_camera_.view_projection);
                data.previous_view_projection = previous_camera_.view_projection;
                data.jitter_xy = current_camera_.jitter;
                data.resolution = glm::vec2{params_.color.image.create_info.extent.width, params_.color.image.create_info.extent.height};
                data.filters = {uint32_t(options.history_filter), uint32_t(options.sample_filter), uint32_t(options.constraint),
                                (options.temporal_filtering ? 1u : 0u) | (options.inverse_luminance_filtering ? 2u : 0u) | (options.luminance_difference_filtering ? 4u : 0u) |
                                    (options.full_taa ? 0u : 8u)};
                data.state = {history_valid_ ? 1u : 0u, params_.depth_channel, 0, 0};
                data.tuning = {options.history_weight, options.depth_threshold, 0, 0};
                barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                                        VK_ACCESS_2_TRANSFER_WRITE_BIT);
                vkz::update(cmd, slot.uniform, data);
                barrier::push(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_UNIFORM_READ_BIT);
                for (auto *image : {&velocity_.image, &history_[0].image, &history_[1].image, &depth_history_[0].image, &depth_history_[1].image}) {
                    barrier::push(*image, color_range, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
                }
                barrier::flush(cmd);
                const auto write = write_index_;
                motion_pipeline_.descriptor_sets = {slot.sets[write]};
                resolve_pipeline_.descriptor_sets = {slot.sets[write]};
                const auto extent = params_.color.image.create_info.extent;
                bind_pipeline(cmd, motion_pipeline_);
                dispatch(cmd, (extent.width + 7) / 8, (extent.height + 7) / 8, 1);
                barrier::push_and_flush(cmd, velocity_.image, color_range, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                                        VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL);
                bind_pipeline(cmd, resolve_pipeline_);
                dispatch(cmd, (extent.width + 7) / 8, (extent.height + 7) / 8, 1);
                const auto color_layout = params_.color.image.layout;
                copy(cmd, history_[write].image, params_.color.image);
                barrier::push(params_.color.image, color_range, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                              color_layout);
                barrier::push(history_[write].image, color_range, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                              VK_IMAGE_LAYOUT_GENERAL);
                barrier::flush(cmd);
                previous_camera_ = current_camera_;
                history_valid_ = true;
                write_index_ = 1 - write;
                updated_ = false;
            }

            texture motion_vectors() const {
                return velocity_;
            }

          private:
            pipeline make_pipeline(const std::vector<uint32_t> &shader) {
                return make_compute_pipeline_builder(params_.device)
                    .shader_stage()
                    .compute_shader(shader)
                    .layout()
                    .add_descriptor_set_layout(layout_)
                    .build();
            }

            void create_resolve_pipeline() {
                if (params_.color.image.create_info.format == VK_FORMAT_R16G16B16A16_SFLOAT) {
                    resolve_pipeline_ = make_pipeline(std::vector<uint32_t>{std::begin(taa_resolve16), std::end(taa_resolve16)});
                } else {
                    resolve_pipeline_ = make_pipeline(std::vector<uint32_t>{std::begin(taa_resolve), std::end(taa_resolve)});
                }
            }

            texture make_target(VkFormat format) {
                texture value{};
                value.image = image::builder(params_.memory_allocator)
                                  .format(format)
                                  .extent(params_.color.image.create_info.extent.width, params_.color.image.create_info.extent.height)
                                  .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                                  .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                                  .build();
                value.image_view =
                    image_view::builder(params_.device)
                        .image(value.image)
                        .view_type(VK_IMAGE_VIEW_TYPE_2D)
                        .format(format)
                        .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                        .level_count(1)
                        .layer_count(1)
                        .build();
                value.sampler = linear_sampler_;
                return value;
            }

            void create_targets() {
                for (auto &target : history_) {
                    target = make_target(params_.color.image.create_info.format);
                }
                for (auto &target : depth_history_) {
                    target = make_target(VK_FORMAT_R32_SFLOAT);
                }
                velocity_ = make_target(VK_FORMAT_R16G16B16A16_SFLOAT);
            }

            void destroy_targets() {
                for (auto *target : {&history_[0], &history_[1], &depth_history_[0], &depth_history_[1], &velocity_}) {
                    target->image_view.destroy();
                    target->image.destroy();
                    *target = {};
                }
            }

            void update_descriptors() {
                for (auto &slot : slots_) {
                    for (uint32_t i = 0; i < 2; ++i) {
                        vkz::update_descriptors(params_.device, {{slot.sets[i],
                                                                  {
                                                                      descriptor<ubo_descriptor>(slot.uniform, 0),
                                                                      texture_descriptor{params_.color.image_view, linear_sampler_, params_.color.image.layout, 1},
                                                                      texture_descriptor{params_.depth.image_view, point_sampler_, params_.depth.image.layout, 2},
                                                                      texture_descriptor{velocity_.image_view, point_sampler_, VK_IMAGE_LAYOUT_GENERAL, 3},
                                                                      texture_descriptor{history_[1 - i].image_view, linear_sampler_, VK_IMAGE_LAYOUT_GENERAL, 4},
                                                                      image_descriptor{velocity_.image_view, VK_IMAGE_LAYOUT_GENERAL, 5},
                                                                      image_descriptor{history_[i].image_view, VK_IMAGE_LAYOUT_GENERAL, 6},
                                                                      texture_descriptor{depth_history_[1 - i].image_view, point_sampler_, VK_IMAGE_LAYOUT_GENERAL, 7},
                                                                      image_descriptor{depth_history_[i].image_view, VK_IMAGE_LAYOUT_GENERAL, 8},
                                                                  }}});
                    }
                }
            }

            struct frame_slot {
                buffer uniform;
                std::array<descriptor_set, 2> sets;
            };

            params params_;
            descriptor_pool pool_;
            descriptor_set_layout layout_;
            sampler linear_sampler_;
            sampler point_sampler_;
            std::vector<frame_slot> slots_;
            std::array<texture, 2> history_;
            std::array<texture, 2> depth_history_;
            texture velocity_;
            pipeline motion_pipeline_;
            pipeline resolve_pipeline_;
            camera current_camera_;
            camera previous_camera_;
            uint32_t current_slot_{};
            uint32_t write_index_{};
            bool history_valid_{};
            bool updated_{};
        };

        std::unordered_map<id, std::unique_ptr<impl>> instances;
        id next_id{1};

        impl &get(id value) {
            const auto it = instances.find(value);
            if (it == instances.end()) {
                throw std::invalid_argument("Invalid TAA id");
            }
            return *it->second;
        }
    } // namespace

    id create(const params &params) {
        auto instance = std::make_unique<impl>(params);
        instance->init();
        const auto value = next_id++;
        instances.emplace(value, std::move(instance));
        return value;
    }

    void destroy(id value) {
        instances.erase(value);
    }

    void resize(id value, const texture &color, const texture &depth) {
        get(value).resize(color, depth);
    }

    void configure(id value, const settings &options) {
        get(value).configure(options);
    }

    void reset(id value) {
        get(value).reset();
    }

    void update(id value, const camera &camera, uint32_t current_frame) {
        get(value).update(camera, current_frame);
    }

    void resolve(id value, VkCommandBuffer cmd) {
        get(value).resolve(cmd);
    }

    texture motion_vectors(id value) {
        return get(value).motion_vectors();
    }

    glm::vec2 jitter(uint64_t frame_index) {
        const auto halton = [](uint32_t index, uint32_t base) {
            float value = 0;
            float scale = 1;
            while (index) {
                scale /= float(base);
                value += scale * float(index % base);
                index /= base;
            }
            return value;
        };
        const auto index = uint32_t(frame_index % 16) + 1;
        return glm::vec2{halton(index, 2), halton(index, 3)} - glm::vec2{0.5f};
    }

    glm::mat4 jitter_projection(const glm::mat4 &projection, glm::vec2 offset, glm::uvec2 resolution) {
        if (!resolution.x || !resolution.y) {
            throw std::invalid_argument("TAA jitter resolution must be nonzero");
        }
        glm::mat4 translation{1};
        translation[3][0] = 2 * offset.x / float(resolution.x);
        translation[3][1] = 2 * offset.y / float(resolution.y);
        return translation * projection;
    }
} // namespace vkz::taa
