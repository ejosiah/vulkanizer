#include "vulkanizer/svsm.hpp"
#include "svsm_manage_comp.hpp"
#include "svsm_shadow_frag.hpp"
#include "svsm_shadow_vert.hpp"
#include "vulkanizer/barrier.hpp"
#include "vulkanizer/builders.hpp"
#include "vulkanizer/commands.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace vkz::svsm {
    namespace {
        constexpr VkImageSubresourceRange color_range{static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1};
        struct uniforms {
            glm::mat4 inverse_view_projection{1};
            std::array<glm::mat4, max_clipmaps> light{};
            std::array<glm::ivec4, max_clipmaps> origin{};
            glm::uvec4 config{}, work{}, limits{};
            glm::vec4 receiver{};
            glm::uvec4 options{};
        };
        static_assert(sizeof(uniforms) == 1424);
        static_assert(sizeof(statistics) == 32);
        static_assert(sizeof(VkDrawIndexedIndirectCommand) == 20);
        bool finite(const glm::vec3 &v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
        bool finite(const glm::mat4 &m) {
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    if (!std::isfinite(m[c][r]))
                        return false;
            return true;
        }
        bool power_two(uint32_t v) { return v && !(v & (v - 1)); }
        bool receiver_format(VkFormat format) {
            switch (format) {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            case VK_FORMAT_R16_SFLOAT:
            case VK_FORMAT_R32_SFLOAT:
            case VK_FORMAT_R16G16B16A16_SFLOAT:
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                return true;
            default:
                return false;
            }
        }

        void synchronize(VkCommandBuffer cmd, vkz::pipeline::stage source, vkz::access source_access, vkz::pipeline::stage target, vkz::access target_access) {
            VkMemoryBarrier2 memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
            memory.srcStageMask = static_cast<VkPipelineStageFlags2>(source);
            memory.srcAccessMask = static_cast<VkAccessFlags2>(source_access);
            memory.dstStageMask = static_cast<VkPipelineStageFlags2>(target);
            memory.dstAccessMask = static_cast<VkAccessFlags2>(target_access);
            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dependency.memoryBarrierCount = 1;
            dependency.pMemoryBarriers = &memory;
            vkCmdPipelineBarrier2(cmd, &dependency);
        }
        void compute_barrier(VkCommandBuffer cmd) {
            synchronize(cmd, vkz::pipeline::stage::compute_shader, vkz::access::shader_write, vkz::pipeline::stage::compute_shader, vkz::access::shader_read_write);
        }
        struct frame_slot {
            buffer uniform, hierarchy, visibility, draws, stats;
            vkz::descriptor_set set;
        };
        class impl {
          public:
            explicit impl(const params &p) : p_(p) {}
            ~impl() {
                compute_.destroy();
                if (graphics_)
                    vkDestroyPipeline(p_.device, graphics_, nullptr);
                if (graphics_layout_)
                    vkDestroyPipelineLayout(p_.device, graphics_layout_, nullptr);
                pool_.destroy();
                layout_.destroy();
                for (auto &s : slots_)
                    for (auto *b : {&s.uniform, &s.hierarchy, &s.visibility, &s.draws, &s.stats})
                        b->destroy();
                pages_.destroy();
                owners_.destroy();
                dummy_.destroy();
                atlas_.image_view.destroy();
                atlas_.image.destroy();
                sampler_.destroy();
            }

            buffer make_buffer(VkDeviceSize size, vkz::buffer::usage extra = vkz::buffer::usage::none) {
                return buffer::builder(p_.memory_allocator)
                    .size(size)
                    .usage(vkz::buffer::usage::storage_transfer | extra)
                    .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                    .build();
            }
            void init() {
                if (!p_.device.logical || !p_.device.physical || !p_.fragment_stores_and_atomics_enabled ||
                    !power_two(p_.virtual_resolution) || !power_two(p_.page_size) || p_.page_size < 8 || p_.page_size > 256 ||
                    p_.virtual_resolution < p_.page_size || p_.virtual_resolution / p_.page_size > 128 || !p_.clipmap_count ||
                    p_.clipmap_count > max_clipmaps || !p_.physical_pages || !p_.max_pages_per_frame || !p_.max_casters ||
                    !p_.in_flight_frames || !std::isfinite(p_.first_clipmap_extent) || p_.first_clipmap_extent <= 0)
                    throw std::invalid_argument("Invalid SVSM settings (fragmentStoresAndAtomics must be enabled)");
                VkPhysicalDeviceProperties properties{};
                vkGetPhysicalDeviceProperties(p_.device, &properties);
                limits_ = properties.limits;
                VkPhysicalDeviceFeatures features{};
                vkGetPhysicalDeviceFeatures(p_.device, &features);
                VkFormatProperties format{};
                vkGetPhysicalDeviceFormatProperties(p_.device, VK_FORMAT_R32_UINT, &format);
                const auto required = static_cast<VkFormatFeatureFlags>(vkz::format_feature::storage_atomic);
                if (!features.fragmentStoresAndAtomics || (format.optimalTilingFeatures & required) != required)
                    throw std::runtime_error("SVSM requires R32_UINT storage image integer atomics in fragment shaders");
                side_ = uint32_t(std::ceil(std::sqrt(double(p_.physical_pages))));
                n_ = p_.virtual_resolution / p_.page_size;
                total_ = n_ * n_ * p_.clipmap_count;
                for (uint32_t width = n_; width; width /= 2)
                    hierarchy_stride_ += width * width;
                const uint64_t atlas_size = uint64_t(side_) * p_.page_size;
                const uint64_t draw_size = uint64_t(p_.max_casters) * p_.clipmap_count * sizeof(VkDrawIndexedIndirectCommand);
                if (atlas_size > limits_.maxImageDimension2D || p_.virtual_resolution > limits_.maxViewportDimensions[0] ||
                    p_.virtual_resolution > limits_.maxViewportDimensions[1] || p_.virtual_resolution > limits_.maxFramebufferWidth ||
                    p_.virtual_resolution > limits_.maxFramebufferHeight || draw_size > limits_.maxStorageBufferRange ||
                    uint64_t(p_.physical_pages) * 4 > limits_.maxStorageBufferRange ||
                    uint64_t(p_.max_casters) * 48 > limits_.maxStorageBufferRange ||
                    (uint64_t(p_.max_casters) + 63) / 64 > limits_.maxComputeWorkGroupCount[0] ||
                    n_ * n_ > limits_.maxComputeWorkGroupCount[0] || limits_.maxPerStageDescriptorStorageBuffers < 9 ||
                    limits_.maxDescriptorSetStorageBuffers < 9 || limits_.maxUniformBufferRange < sizeof(uniforms))
                    throw std::invalid_argument("SVSM configuration exceeds physical device limits");
                auto builder = make_descriptor_set_layout_builder(p_.device);
                for (uint32_t b = 0; b < 12; ++b) {
                    const auto type = b == 0   ? vkz::descriptor::type::uniform_buffer
                                      : b == 2 ? vkz::descriptor::type::storage_image
                                      : b == 3 ? vkz::descriptor::type::combined_image_sampler
                                               : vkz::descriptor::type::storage_buffer;
                    auto entry = builder.binding(b).descriptor_type(type).descriptor_count(1).shader_stages(
                        (b == 0 || b == 1 || b == 2 || b == 7 || b == 10) ? vkz::shader_stage::compute | vkz::shader_stage::vertex | vkz::shader_stage::fragment : vkz::shader_stage::compute);
                    if (b == 11)
                        layout_ = entry.create_layout();
                    else
                        entry.build();
                }
                pool_ = descriptor_pool{p_.device,
                                        p_.in_flight_frames,
                                        {{static_cast<VkDescriptorType>(vkz::descriptor::type::uniform_buffer), p_.in_flight_frames},
                                         {static_cast<VkDescriptorType>(vkz::descriptor::type::storage_buffer), p_.in_flight_frames * 9},
                                         {static_cast<VkDescriptorType>(vkz::descriptor::type::storage_image), p_.in_flight_frames},
                                         {static_cast<VkDescriptorType>(vkz::descriptor::type::combined_image_sampler), p_.in_flight_frames}}};
                sampler_ = sampler::builder(p_.device)
                               .min_filter(VK_FILTER_NEAREST)
                               .mag_filter(VK_FILTER_NEAREST)
                               .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
                               .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                               .build();
                atlas_.image = image::builder(p_.memory_allocator)
                    .format(VK_FORMAT_R32_UINT)
                    .extent(uint32_t(atlas_size), uint32_t(atlas_size))
                    .usage(vkz::image::usage::storage_download)
                    .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                    .build();
                atlas_.image_view = image_view::builder(p_.device)
                    .image(atlas_.image)
                    .format(VK_FORMAT_R32_UINT)
                    .view_type(VK_IMAGE_VIEW_TYPE_2D)
                    .aspect_mask(vkz::image::aspect::color)
                    .level_count(1)
                    .layer_count(1)
                    .build();
                pages_ = make_buffer(VkDeviceSize(total_) * 16);
                owners_ = make_buffer(VkDeviceSize(p_.physical_pages) * 4);
                dummy_ = make_buffer(64);
                slots_.resize(p_.in_flight_frames);
                for (auto &s : slots_) {
                    s.uniform = make_buffer(sizeof(uniforms), vkz::buffer::usage::uniform);
                    s.hierarchy = make_buffer(VkDeviceSize(hierarchy_stride_) * p_.clipmap_count * 4);
                    s.visibility = make_buffer(VkDeviceSize(p_.max_casters) * p_.clipmap_count * 4);
                    s.draws = make_buffer(draw_size, vkz::buffer::usage::indirect);
                    s.stats = make_buffer(sizeof(statistics));
                    s.set = pool_.allocate(layout_);
                }
                compute_ = make_compute_pipeline_builder(p_.device)
                               .shader_stage()
                               .compute_shader(std::vector<uint32_t>{std::begin(svsm_manage_comp), std::end(svsm_manage_comp)})
                               .layout()
                               .add_descriptor_set_layout(layout_)
                               .add_push_constant_range(vkz::shader_stage::compute, 0, 16)
                               .build();
                make_graphics();
            }
            void make_graphics() {
                VkPushConstantRange push{static_cast<VkShaderStageFlags>(vkz::shader_stage::vertex | vkz::shader_stage::fragment), 0, sizeof(draw_constants)};
                VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
                layout.setLayoutCount = 1;
                layout.pSetLayouts = &layout_.handle;
                layout.pushConstantRangeCount = 1;
                layout.pPushConstantRanges = &push;
                VKZ_CHECK_VULKAN(vkCreatePipelineLayout(p_.device, &layout, nullptr, &graphics_layout_));
                std::array<VkShaderModule, 2> modules{};
                try {
                    VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                    shader.codeSize = sizeof(svsm_shadow_vert);
                    shader.pCode = svsm_shadow_vert;
                    VKZ_CHECK_VULKAN(vkCreateShaderModule(p_.device, &shader, nullptr, &modules[0]));
                    shader.codeSize = sizeof(svsm_shadow_frag);
                    shader.pCode = svsm_shadow_frag;
                    VKZ_CHECK_VULKAN(vkCreateShaderModule(p_.device, &shader, nullptr, &modules[1]));
                    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
                    for (uint32_t i = 0; i < 2; ++i) {
                        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                        stages[i].stage = static_cast<VkShaderStageFlagBits>(i ? vkz::shader_stage::fragment : vkz::shader_stage::vertex);
                        stages[i].module = modules[i];
                        stages[i].pName = "main";
                    }
                    VkVertexInputBindingDescription binding{0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX};
                    VkVertexInputAttributeDescription attribute{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
                    VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
                    vertex.vertexBindingDescriptionCount = 1;
                    vertex.pVertexBindingDescriptions = &binding;
                    vertex.vertexAttributeDescriptionCount = 1;
                    vertex.pVertexAttributeDescriptions = &attribute;
                    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
                    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                    VkViewport viewport{0, 0, float(p_.virtual_resolution), float(p_.virtual_resolution), 0, 1};
                    VkRect2D scissor{{0, 0}, {p_.virtual_resolution, p_.virtual_resolution}};
                    VkPipelineViewportStateCreateInfo view{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
                    view.viewportCount = 1;
                    view.pViewports = &viewport;
                    view.scissorCount = 1;
                    view.pScissors = &scissor;
                    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
                    raster.polygonMode = VK_POLYGON_MODE_FILL;
                    raster.cullMode = static_cast<VkCullModeFlags>(vkz::cull_mode::none);
                    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
                    raster.lineWidth = 1;
                    VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
                    samples.rasterizationSamples = static_cast<VkSampleCountFlagBits>(vkz::sample_count::x1);
                    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
                    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
                    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
                    info.pNext = &rendering;
                    info.stageCount = 2;
                    info.pStages = stages.data();
                    info.pVertexInputState = &vertex;
                    info.pInputAssemblyState = &assembly;
                    info.pViewportState = &view;
                    info.pRasterizationState = &raster;
                    info.pMultisampleState = &samples;
                    info.pColorBlendState = &blend;
                    info.layout = graphics_layout_;
                    VKZ_CHECK_VULKAN(vkCreateGraphicsPipelines(p_.device, VK_NULL_HANDLE, 1, &info, nullptr, &graphics_));
                } catch (...) {
                    for (auto m : modules)
                        if (m)
                            vkDestroyShaderModule(p_.device, m, nullptr);
                    throw;
                }
                for (auto m : modules)
                    vkDestroyShaderModule(p_.device, m, nullptr);
            }
            void check_buffer(const buffer &b, uint64_t size, const char *message) {
                if (size && (!b._ || b.create_info.size < size || !(b.create_info.usage & static_cast<VkBufferUsageFlags>(vkz::buffer::usage::storage)) ||
                             b.create_info.size > limits_.maxStorageBufferRange))
                    throw std::invalid_argument(message);
            }
            void begin(VkCommandBuffer cmd, const frame_inputs &f) {
                if (state_ != 0)
                    throw std::logic_error("SVSM previous begin requires capture and end");
                if (!cmd || f.frame_slot >= slots_.size() || f.caster_count > p_.max_casters || f.depth_channel > 3 ||
                    f.filter_radius > 4 || !finite(f.inverse_view_projection) ||
                    !std::isfinite(glm::determinant(f.inverse_view_projection)) ||
                    std::abs(glm::determinant(f.inverse_view_projection)) < 1e-20f || !finite(f.camera_position) ||
                    !finite(f.light_direction) || glm::dot(f.light_direction, f.light_direction) < 1e-12f ||
                    !std::isfinite(f.light_depth_min) || !std::isfinite(f.light_depth_max) || f.light_depth_max <= f.light_depth_min ||
                    !std::isfinite(f.light_depth_max - f.light_depth_min) || !std::isfinite(f.background_depth))
                    throw std::invalid_argument("Invalid SVSM frame inputs");
                const auto &depth = f.depth.image.create_info;
                if (!f.depth.image.handle || !f.depth.image_view.handle || !depth.extent.width || !depth.extent.height ||
                    !receiver_format(f.depth.image_view.create_info.format) ||
                    f.depth.image_view.create_info.viewType != VK_IMAGE_VIEW_TYPE_2D || depth.imageType != VK_IMAGE_TYPE_2D ||
                    depth.samples != static_cast<VkSampleCountFlagBits>(vkz::sample_count::x1) || !(depth.usage & static_cast<VkImageUsageFlags>(vkz::image::usage::sampled)) ||
                    (f.depth.image.layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                     f.depth.image.layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL &&
                     f.depth.image.layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL && f.depth.image.layout != VK_IMAGE_LAYOUT_GENERAL) ||
                    (depth.extent.width + 63) / 64 > limits_.maxComputeWorkGroupCount[0] ||
                    depth.extent.height > limits_.maxComputeWorkGroupCount[1])
                    throw std::invalid_argument("SVSM receiver depth must be a sampled single-sample 2D image in a readable layout");
                check_buffer(f.casters, uint64_t(f.caster_count) * sizeof(caster_bounds),
                             "SVSM caster buffer is missing, undersized or lacks storage usage");
                check_buffer(f.invalidations, uint64_t(f.invalidation_count) * sizeof(invalidation),
                             "SVSM invalidation buffer is missing or undersized");
                if ((uint64_t(f.invalidation_count) + 63) / 64 > limits_.maxComputeWorkGroupCount[0])
                    throw std::invalid_argument("Too many SVSM invalidations");
                if (f.indexed_draws._)
                    check_buffer(f.indexed_draws, uint64_t(f.caster_count) * 20,
                                 "SVSM indexed templates are undersized or lack storage usage");
                uniforms u{};
                u.inverse_view_projection = f.inverse_view_projection;
                const glm::vec3 direction = glm::vec3(glm::normalize(glm::dvec3(f.light_direction)));
                const glm::vec3 seed = std::abs(direction.y) > 0.99f ? glm::vec3{1, 0, 0} : glm::vec3{0, 1, 0};
                const auto right = glm::normalize(glm::cross(direction, seed)), up = glm::cross(right, direction);
                bool reset = reset_ || f.reset_cache || direction != last_direction_ || f.light_depth_min != last_min_ ||
                             f.light_depth_max != last_max_;
                for (uint32_t level = 0; level < p_.clipmap_count; ++level) {
                    const double extent = std::ldexp(double(p_.first_clipmap_extent), int(level)), page = extent / n_;
                    const double cx = std::floor(double(glm::dot(right, f.camera_position)) / page) - n_ / 2;
                    const double cy = std::floor(double(glm::dot(up, f.camera_position)) / page) - n_ / 2;
                    if (!std::isfinite(cx) || !std::isfinite(cy) || std::abs(cx) > 100000000 || std::abs(cy) > 100000000)
                        throw std::invalid_argument("SVSM coordinates exceed page addressing range; rebase the world");
                    u.origin[level] = {int(cx), int(cy), 0, 0};
                    glm::mat4 m{1};
                    for (int axis = 0; axis < 3; ++axis) {
                        m[axis][0] = right[axis] * float(2 / extent);
                        m[axis][1] = up[axis] * float(2 / extent);
                        m[axis][2] = direction[axis] / (f.light_depth_max - f.light_depth_min);
                    }
                    m[3][0] = float(-(cx + n_ * 0.5) * 2 / n_);
                    m[3][1] = float(-(cy + n_ * 0.5) * 2 / n_);
                    m[3][2] = -f.light_depth_min / (f.light_depth_max - f.light_depth_min);
                    if (!finite(m))
                        throw std::invalid_argument("SVSM clipmap transform exceeds floating-point range");
                    u.light[level] = m;
                }
                u.config = {p_.virtual_resolution, p_.page_size, p_.clipmap_count, side_};
                u.work = {f.caster_count, f.invalidation_count, reset ? 1u : 0u, p_.cache ? 1u : 0u};
                u.limits = {p_.max_pages_per_frame, p_.physical_pages, hierarchy_stride_, 0};
                u.receiver = {f.background_depth, 0, 0, 0};
                u.options = {f.depth_channel, f.filter_radius, f.request_all_pages ? 1u : 0u, f.indexed_draws._ ? 1u : 0u};
                auto &s = slots_[f.frame_slot];
                const auto &casters = f.caster_count ? f.casters : dummy_;
                const auto &changes = f.invalidation_count ? f.invalidations : dummy_;
                const auto &templates = f.caster_count && f.indexed_draws._ ? f.indexed_draws : dummy_;
                update_descriptors(p_.device, {{s.set,
                                                {make_descriptor<ubo_descriptor>(s.uniform, 0), make_descriptor<buffer_descriptor>(pages_, 1),
                                                 image_descriptor{atlas_.image_view, VK_IMAGE_LAYOUT_GENERAL, 2}, texture_descriptor{f.depth.image_view, sampler_, f.depth.image.layout, 3},
                                                 make_descriptor<buffer_descriptor>(casters, 4), make_descriptor<buffer_descriptor>(changes, 5), make_descriptor<buffer_descriptor>(s.hierarchy, 6),
                                                 make_descriptor<buffer_descriptor>(s.visibility, 7), make_descriptor<buffer_descriptor>(templates, 8), make_descriptor<buffer_descriptor>(s.draws, 9),
                                                 make_descriptor<buffer_descriptor>(owners_, 10), make_descriptor<buffer_descriptor>(s.stats, 11)}}});
                // Includes previous-frame shader READS: persistent pages must not be reassigned early.
                synchronize(cmd, vkz::pipeline::stage::all_commands, vkz::access::memory_read_write, vkz::pipeline::stage::transfer | vkz::pipeline::stage::compute_shader,
                            vkz::access::transfer_write | vkz::access::shader_read | vkz::access::shader_write);
                vkCmdUpdateBuffer(cmd, s.uniform, 0, sizeof(u), &u);
                vkCmdFillBuffer(cmd, s.stats, 0, VK_WHOLE_SIZE, 0);
                if (reset)
                    vkCmdFillBuffer(cmd, owners_, 0, VK_WHOLE_SIZE, ~0u);
                // A reset discards all contents, including recovery after an abandoned first recording.
                if (reset)
                    atlas_.image.layout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier::push_and_flush(cmd, atlas_.image, color_range, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::compute_shader, vkz::access::memory_read_write,
                                        vkz::access::shader_read_write, VK_IMAGE_LAYOUT_GENERAL);
                synchronize(cmd, vkz::pipeline::stage::transfer, vkz::access::transfer_write, vkz::pipeline::stage::compute_shader,
                            vkz::access::uniform_read | vkz::access::shader_read | vkz::access::shader_write);
                slot_ = f.frame_slot;
                input_ = f;
                current_ = u;
                command_ = cmd;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_.handle);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_.layout, 0, 1, &s.set.handle, 0, nullptr);
                dispatch(cmd, 0, (total_ + 63) / 64);
                dispatch(cmd, 1, (depth.extent.width + 63) / 64, depth.extent.height);
                dispatch(cmd, 2, (f.invalidation_count + 63) / 64, p_.clipmap_count);
                timed(cmd, 3, [&] {
                    dispatch(cmd, 8, (std::max(total_, p_.physical_pages) + 63) / 64);
                    dispatch(cmd, 3, 1);
                });
                dispatch(cmd, 4, n_ * n_, p_.clipmap_count);
                timed(cmd, 5, [&] {
                    uint32_t offset = 0, previous = 0;
                    for (uint32_t width = n_; width; width /= 2) {
                        dispatch(cmd, 5, (width * width + 63) / 64, p_.clipmap_count, {offset, previous, width});
                        previous = offset;
                        offset += width * width;
                    }
                });
                dispatch(cmd, 6, (f.caster_count + 63) / 64, p_.clipmap_count);
                synchronize(cmd, vkz::pipeline::stage::compute_shader | vkz::pipeline::stage::transfer, vkz::access::shader_write | vkz::access::transfer_write,
                            vkz::pipeline::stage::all_graphics | vkz::pipeline::stage::draw_indirect,
                            vkz::access::shader_read | vkz::access::shader_write | vkz::access::uniform_read | vkz::access::indirect_command_read);
                state_ = 1;
                reset_ = false;
                last_direction_ = direction;
                last_min_ = f.light_depth_min;
                last_max_ = f.light_depth_max;
            }
            template <typename Body> void timed(VkCommandBuffer cmd, uint32_t stage, Body &&body) {
                if (input_.gpu_profiler)
                    input_.gpu_profiler->profile(profiling_queries[stage], cmd, std::forward<Body>(body), vkz::pipeline::stage::bottom_of_pipe);
                else
                    body();
            }
            void dispatch(VkCommandBuffer cmd, uint32_t mode, uint32_t x, uint32_t y = 1, glm::uvec3 args = {}) {
                auto body = [&] {
                    if (!x || !y)
                        return;
                    glm::uvec4 push{mode, args.x, args.y, args.z};
                    vkCmdPushConstants(cmd, compute_.layout, static_cast<VkShaderStageFlags>(vkz::shader_stage::compute), 0, 16, &push);
                    vkCmdDispatch(cmd, x, y, 1);
                    compute_barrier(cmd);
                };
                if (mode == 3 || mode == 5 || mode == 8)
                    body();
                else
                    timed(cmd, mode, body);
            }
            void capture(VkCommandBuffer cmd, const std::function<void(const render_context &)> &draw) {
                if (state_ != 1 || cmd != command_ || !draw)
                    throw std::logic_error("SVSM capture must follow begin on the same command buffer");
                timed(cmd, 8, [&] {
                    auto &s = slots_[slot_];
                    for (uint32_t level = 0; level < p_.clipmap_count; ++level) {
                        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
                        rendering.renderArea.extent = {p_.virtual_resolution, p_.virtual_resolution};
                        rendering.layerCount = 1;
                        vkCmdBeginRendering(cmd, &rendering);
                        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_);
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_layout_, 0, 1, &s.set.handle, 0, nullptr);
                        draw_constants constants{};
                        constants.clipmap = level;
                        constants.use_visibility = 0;
                        vkCmdPushConstants(cmd, graphics_layout_, static_cast<VkShaderStageFlags>(vkz::shader_stage::vertex | vkz::shader_stage::fragment), 0, sizeof(constants), &constants);
                        try {
                            draw({cmd, level, current_.light[level], s.set.handle, graphics_layout_, s.visibility,
                                  input_.indexed_draws._ ? s.draws : buffer{}, VkDeviceSize(level) * input_.caster_count * 20,
                                  input_.caster_count});
                        } catch (...) {
                            vkCmdEndRendering(cmd);
                            state_ = 0;
                            reset_ = true;
                            throw;
                        }
                        vkCmdEndRendering(cmd);
                    }
                });
                state_ = 2;
            }
            void end(VkCommandBuffer cmd) {
                if (state_ != 2 || cmd != command_)
                    throw std::logic_error("SVSM end must follow capture on the same command buffer");
                synchronize(cmd, vkz::pipeline::stage::all_graphics, vkz::access::shader_read_write, vkz::pipeline::stage::compute_shader, vkz::access::shader_read_write);
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_.handle);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_.layout, 0, 1, &slots_[slot_].set.handle, 0, nullptr);
                dispatch(cmd, 7, (total_ + 63) / 64);
                synchronize(cmd, vkz::pipeline::stage::compute_shader | vkz::pipeline::stage::fragment_shader, vkz::access::shader_write, vkz::pipeline::stage::all_commands,
                            vkz::access::shader_read | vkz::access::uniform_read | vkz::access::transfer_read);
                state_ = 0;
            }
            frame_slot &slot(uint32_t s) {
                if (s >= slots_.size())
                    throw std::out_of_range("Invalid SVSM frame slot");
                return slots_[s];
            }
            params p_;
            VkPhysicalDeviceLimits limits_{};
            descriptor_pool pool_;
            vkz::descriptor_set_layout layout_;
            sampler sampler_;
            texture atlas_;
            buffer pages_, owners_, dummy_;
            pipeline compute_;
            VkPipeline graphics_{};
            VkPipelineLayout graphics_layout_{};
            std::vector<frame_slot> slots_;
            uniforms current_{};
            frame_inputs input_{};
            glm::vec3 last_direction_{};
            float last_min_{}, last_max_{};
            uint32_t n_{}, total_{}, side_{}, hierarchy_stride_{}, slot_{}, state_{};
            bool reset_{true};
            VkCommandBuffer command_{};
        };
        std::unordered_map<id, std::unique_ptr<impl>> instances;
        id next_id = 1;
        impl &get(id value) {
            auto it = instances.find(value);
            if (it == instances.end())
                throw std::invalid_argument("Invalid SVSM id");
            return *it->second;
        }
    } // namespace
    id create(const params &p) {
        auto instance = std::make_unique<impl>(p);
        instance->init();
        auto value = next_id++;
        instances.emplace(value, std::move(instance));
        return value;
    }
    void destroy(id value) { instances.erase(value); }
    void begin(id value, VkCommandBuffer cmd, const frame_inputs &inputs) { get(value).begin(cmd, inputs); }
    void capture(id value, VkCommandBuffer cmd, const std::function<void(const render_context &)> &draw) { get(value).capture(cmd, draw); }
    void end(id value, VkCommandBuffer cmd) { get(value).end(cmd); }
    void reset(id value) {
        auto &instance = get(value);
        instance.reset_ = true;
        instance.state_ = 0;
    }
    VkDescriptorSetLayout descriptor_set_layout(id value) { return get(value).layout_.handle; }
    VkDescriptorSet descriptor_set(id value, uint32_t slot) { return get(value).slot(slot).set.handle; }
    buffer visibility(id value, uint32_t slot) { return get(value).slot(slot).visibility; }
    buffer indexed_draws(id value, uint32_t slot) { return get(value).slot(slot).draws; }
    buffer counters(id value, uint32_t slot) { return get(value).slot(slot).stats; }
    texture physical_pool(id value) { return get(value).atlas_; }
} // namespace vkz::svsm
