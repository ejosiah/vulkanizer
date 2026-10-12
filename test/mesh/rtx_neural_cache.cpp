#include "rtx_support.hpp"
#include "rtx_neural_cache.hpp"
#include <vulkanizer/builders.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/compute_pipeline_builder.hpp>
#include <NrcVk.h>
#include <cmrc/cmrc.hpp>
#include <array>
#include <cstring>
#include <iostream>

CMRC_DECLARE(rtx_shaders);

namespace rtx {
    struct neural_radiance_cache::implementation {
        implementation(vkz::context &context, uint32_t width, uint32_t height) : device(context.device), instance(context.instance), allocator(vkz::vma_memory_allocator::create(context)) {
            configuration.includeDirectLighting = true;
            configuration.frameDimensions = {.x = width, .y = height};
            configuration.trainingDimensions = nrc::ComputeIdealTrainingDimensions(configuration.frameDimensions, 4, 0.0f);
            create_layout();
            create_pipelines();
            create_buffers();
            update_descriptors();
        }

        ~implementation() {
            vkDeviceWaitIdle(device);
            for (auto &pipeline : pipelines)
                pipeline.destroy();
            descriptors.destroy();
            descriptor_layout.destroy();
            destroy_buffers();
            if (cache) {
                const auto status = nrc::vulkan::Context::Destroy(*cache);
                if (status != nrc::Status::OK) vkz::error(std::format("NRC context destruction failed: {}", int(status)));
            }
            if (initialized) nrc::vulkan::Shutdown();
            allocator.destroy();
        }

        void create_context() {
            nrc::GlobalSettings global{.loggerFn =
                                           [](const char *message, nrc::LogLevel) {
                                               std::cerr << "NRC: " << message << '\n';
                                           },
                                       .enableDebugBuffers = true,
                                       .maxNumFramesInFlight = 1};
            VKZ_ASSERT(nrc::vulkan::Initialize(global) == nrc::Status::OK, "NRC initialization failed");
            initialized = true;
            const auto status = nrc::vulkan::Context::Create(device, device.physical, instance, cache);
            if (status != nrc::Status::OK) {
                nrc::vulkan::Shutdown();
                initialized = false;
                VKZ_THROW("NRC context creation failed");
            }
        }

        void configure(glm::vec4 bounds, uint32_t bounces, bool reset) {
            if (!cache) create_context();

            auto next = configuration;
            next.sceneBoundsMin = {.x = bounds.x - bounds.w, .y = bounds.y - bounds.w, .z = bounds.z - bounds.w};
            next.sceneBoundsMax = {.x = bounds.x + bounds.w, .y = bounds.y + bounds.w, .z = bounds.z + bounds.w};
            next.maxPathVertices = bounces;
            next.requestReset = reset;
            if (configured && !reset && next == configuration) return;

            VKZ_CHECK_VULKAN(vkDeviceWaitIdle(device));
            VKZ_ASSERT(cache->Configure(next) == nrc::Status::OK, "NRC configuration failed");
            configuration = next;
            configuration.requestReset = false;
            destroy_buffers();
            create_buffers();
            update_descriptors();
            configured = true;
            frames = 0;
        }

        void begin_frame(VkCommandBuffer command_buffer) {
            nrc::FrameSettings settings{.skipDeltaVertices = true, .proportionUnbiased = 1.0f};
            barrier(command_buffer);
            VKZ_ASSERT(cache->BeginFrame(command_buffer, settings) == nrc::Status::OK, "NRC BeginFrame failed");
            NrcConstants constants{};
            VKZ_ASSERT(cache->PopulateShaderConstants(constants) == nrc::Status::OK, "NRC constants failed");
            vkz::update(command_buffer, buffers[0], constants);
            barrier(command_buffer);
            frame_pending = true;
        }

        void resolve(VkCommandBuffer command_buffer, vkz::image &output, vkz::image_view view) {
            barrier(command_buffer);
            vkz::bind_pipeline(command_buffer, pipelines[0]);
            vkz::bind_descriptor_sets(command_buffer, pipelines[0], {descriptor_set});
            profile_pass(command_buffer, "Training bridge", [&] { vkz::dispatch(command_buffer, (configuration.trainingDimensions.x + 7) / 8, (configuration.trainingDimensions.y + 7) / 8); });
            barrier(command_buffer);
            vkz::bind_pipeline(command_buffer, pipelines[1]);
            vkz::bind_descriptor_sets(command_buffer, pipelines[1], {descriptor_set});
            profile_pass(command_buffer, "Query bridge", [&] { vkz::dispatch(command_buffer, (configuration.frameDimensions.x + 7) / 8, (configuration.frameDimensions.y + 7) / 8); });
            barrier(command_buffer);
            profile_pass(command_buffer, "Query and train", [&] { VKZ_ASSERT(cache->QueryAndTrain(command_buffer, nullptr) == nrc::Status::OK, "NRC QueryAndTrain failed"); });
            barrier(command_buffer);
            const VkImageSubresourceRange range{.aspectMask = static_cast<VkImageAspectFlags>(vkz::image::aspect::color), .levelCount = 1, .layerCount = 1};
            vkz::barrier::push_and_flush(command_buffer, output, range, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::compute_shader, vkz::access::memory_write,
                                         vkz::access::shader_read_write, VK_IMAGE_LAYOUT_GENERAL);
            profile_pass(command_buffer, "Composite", [&] { VKZ_ASSERT(cache->Resolve(command_buffer, view) == nrc::Status::OK, "NRC resolve failed"); });
        }

        void end_frame(VkQueue queue) {
            if (!frame_pending) return;
            VKZ_ASSERT(cache->EndFrame(queue) == nrc::Status::OK, "NRC EndFrame failed");
            frame_pending = false;
            ++frames;
        }

        void create_layout() {
            descriptor_layout = vkz::descriptor_set_layout_builder{device}
                                    .binding(0)
                                    .descriptor_type(vkz::descriptor::type::storage_buffer)
                                    .descriptor_count(1)
                                    .shader_stages(vkz::shader_stage::compute | vkz::shader_stage::raygen)
                                    .binding(1)
                                    .descriptor_type(vkz::descriptor::type::storage_buffer)
                                    .descriptor_count(1)
                                    .shader_stages(vkz::shader_stage::compute | vkz::shader_stage::raygen)
                                    .binding(2)
                                    .descriptor_type(vkz::descriptor::type::storage_buffer)
                                    .descriptor_count(1)
                                    .shader_stages(vkz::shader_stage::compute | vkz::shader_stage::raygen)
                                    .binding(3)
                                    .descriptor_type(vkz::descriptor::type::storage_buffer)
                                    .descriptor_count(1)
                                    .shader_stages(vkz::shader_stage::compute | vkz::shader_stage::raygen)
                                    .binding(4)
                                    .descriptor_type(vkz::descriptor::type::storage_buffer)
                                    .descriptor_count(1)
                                    .shader_stages(vkz::shader_stage::compute | vkz::shader_stage::raygen)
                                    .binding(5)
                                    .descriptor_type(vkz::descriptor::type::storage_buffer)
                                    .descriptor_count(1)
                                    .shader_stages(vkz::shader_stage::compute | vkz::shader_stage::raygen)
                                    .binding(6)
                                    .descriptor_type(vkz::descriptor::type::storage_buffer)
                                    .descriptor_count(1)
                                    .shader_stages(vkz::shader_stage::compute | vkz::shader_stage::raygen)
                                    .binding(7)
                                    .descriptor_type(vkz::descriptor::type::storage_buffer)
                                    .descriptor_count(1)
                                    .shader_stages(vkz::shader_stage::compute | vkz::shader_stage::raygen)
                                    .binding(8)
                                    .descriptor_type(vkz::descriptor::type::storage_buffer)
                                    .descriptor_count(1)
                                    .shader_stages(vkz::shader_stage::compute | vkz::shader_stage::raygen)
                                    .create_layout();
            descriptors = vkz::descriptor_pool(device, 1, {{.type = static_cast<VkDescriptorType>(vkz::descriptor::type::storage_buffer), .descriptorCount = 9}});
            descriptor_set = descriptors.allocate(descriptor_layout);
        }

        void create_pipelines() {
            const char *names[] = {"nrc_train_bridge.spv", "nrc_query_bridge.spv"};
            for (size_t i = 0; i < pipelines.size(); ++i) {
                const auto file = cmrc::rtx_shaders::get_filesystem().open(names[i]);
                std::vector<uint32_t> shader(file.size() / sizeof(uint32_t));
                std::memcpy(shader.data(), file.begin(), file.size());
                pipelines[i] =
                    vkz::compute_pipeline_builder{device}
                        .shader_stage()
                            .compute_shader(shader)
                        .layout()
                            .add_descriptor_set_layout(descriptor_layout)
                    .build();
            }
        }

        void create_buffers() {
            const VkDeviceSize pixel_count = VkDeviceSize(configuration.frameDimensions.x) * configuration.frameDimensions.y;
            const VkDeviceSize training_count = VkDeviceSize(configuration.trainingDimensions.x) * configuration.trainingDimensions.y;
            const VkDeviceSize sizes[] = {sizeof(NrcConstants), pixel_count * sizeof(vertex_record), training_count * configuration.maxPathVertices * sizeof(vertex_record),
                                         training_count * sizeof(training_path)};
            for (size_t i = 0; i < buffers.size(); ++i) {
                buffers[i] = vkz::buffer::builder(allocator)
                    .size(sizes[i])
                    .usage(vkz::buffer::usage::storage_upload)
                    .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                    .build();
            }
        }

        void destroy_buffers() {
            for (auto &buffer : buffers) {
                buffer.destroy();
                buffer = {};
            }
        }

        void update_descriptors() {
            vkz::descriptor_bindings bindings{.descriptor_set = descriptor_set};
            for (uint32_t i = 0; i < buffers.size(); ++i)
                bindings.bindings.push_back(vkz::make_descriptor<vkz::buffer_descriptor>(buffers[i], i));

            const nrc::BufferIdx shared[] = {nrc::BufferIdx::Counter, nrc::BufferIdx::QueryPathInfo,
                nrc::BufferIdx::TrainingPathInfo, nrc::BufferIdx::TrainingPathVertices, nrc::BufferIdx::QueryRadianceParams};
            const auto *resources = cache ? cache->GetBuffers() : nullptr;
            for (uint32_t i = 0; i < std::size(shared); ++i) {
                auto binding = vkz::make_descriptor<vkz::buffer_descriptor>(buffers[0], i + 4);
                if (resources) {
                    const auto &resource = (*resources)[shared[i]];
                    binding.buffer._ = resource.resource;
                    binding.end = resource.allocatedSize;
                }
                bindings.bindings.push_back(binding);
            }
            vkz::update_descriptor(device, std::move(bindings));
        }

        void barrier(VkCommandBuffer command_buffer) {
            vkz::barrier::push_and_flush(command_buffer, vkz::pipeline::stage::all_commands, vkz::pipeline::stage::all_commands, vkz::access::memory_read_write, vkz::access::memory_read_write);
        }

        struct vertex_record {
            glm::vec4 position_roughness;
            glm::vec4 normal_distance;
            glm::vec4 view_delta;
            glm::vec4 diffuse_reflectance;
            glm::vec4 specular_reflectance;
            glm::vec4 direct_radiance;
            glm::vec4 scatter_weight_pdf;
            glm::vec4 throughput_valid;
            glm::vec4 emission;
        };

        struct training_path {
            glm::vec4 terminal_radiance;
            uint32_t vertex_count;
            uint32_t exited;
            uint32_t reserved_0;
            uint32_t reserved_1;
        };

        static_assert(sizeof(vertex_record) == 144);
        static_assert(sizeof(training_path) == 32);

        vkz::device device;
        VkInstance instance;
        bool initialized{};
        vkz::vma_memory_allocator allocator;
        nrc::vulkan::Context *cache{};
        nrc::ContextSettings configuration;
        bool configured{};
        bool frame_pending{};
        uint32_t frames{};
        std::array<vkz::buffer, 4> buffers;
        vkz::descriptor_pool descriptors;
        vkz::descriptor_set_layout descriptor_layout;
        vkz::descriptor_set descriptor_set;
        std::array<vkz::pipeline, 2> pipelines;
    };

    neural_radiance_cache::neural_radiance_cache(vkz::context &context, uint32_t width, uint32_t height) : state_(std::make_unique<implementation>(context, width, height)) {
    }

    neural_radiance_cache::~neural_radiance_cache() = default;

    void neural_radiance_cache::configure(glm::vec4 bounds, uint32_t bounces, bool reset) {
        state_->configure(bounds, bounces, reset);
    }

    void neural_radiance_cache::begin_frame(VkCommandBuffer command_buffer) {
        state_->begin_frame(command_buffer);
    }

    void neural_radiance_cache::resolve(VkCommandBuffer command_buffer, vkz::image &output, vkz::image_view view) {
        state_->resolve(command_buffer, output, view);
    }

    void neural_radiance_cache::end_frame(VkQueue queue) {
        state_->end_frame(queue);
    }

    vkz::descriptor_set_layout neural_radiance_cache::layout() const {
        return state_->descriptor_layout;
    }

    vkz::descriptor_set neural_radiance_cache::bindings() const {
        return state_->descriptor_set;
    }

    glm::uvec2 neural_radiance_cache::training_dimensions() const {
        return {state_->configuration.trainingDimensions.x, state_->configuration.trainingDimensions.y};
    }

    bool neural_radiance_cache::ready() const {
        return state_->frames >= 16;
    }
}
