#include "rtx_support.hpp"
#include "rtx_nrd.hpp"
#include <NRD.h>
#include <vulkanizer/builders.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/compute_pipeline_builder.hpp>
#include <vulkanizer/status.hpp>
#include <cmrc/cmrc.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <map>
#include <vector>
#include <cstring>

CMRC_DECLARE(rtx_shaders);

namespace rtx {
    struct nrd_denoiser::implementation {
        implementation(vkz::context &context, glm::uvec2 resolution, const input &inputs, int shadow_light)
            : device(context.device), allocator(vkz::vma_memory_allocator::create(context)), width(resolution.x), height(resolution.y), shadow_light(shadow_light) {
            create_instance();
            create_images();
            create_layouts();
            create_input_descriptors(inputs);
            create_pipelines();
        }

        ~implementation() {
            vkDeviceWaitIdle(device);
            for (auto &pipeline : pipelines)
                pipeline.destroy();
            for (auto &pipeline : bridge_pipelines)
                pipeline.destroy();
            descriptors.destroy();
            for (auto &layout : layouts)
                layout.destroy();
            bridge_layout.destroy();
            input_layout.destroy();
            frame_buffer.destroy();
            input_sampler.destroy();
            for (auto &sampler : samplers)
                sampler.destroy();
            constants.destroy();
            for (auto &texture : images)
                texture.destroy();
            for (auto &texture : permanent)
                texture.destroy();
            for (auto &texture : transient)
                texture.destroy();
            if (instance) nrd::DestroyInstance(*instance);
            allocator.destroy();
        }

        void create_instance() {
            const nrd::DenoiserDesc denoiser{.identifier = 0, .denoiser = shadow_light == -3 ? nrd::Denoiser::RELAX_DIFFUSE
                : shadow_light == -2 ? nrd::Denoiser::RELAX_SPECULAR
                : shadow_light < 0 ? nrd::Denoiser::RELAX_DIFFUSE_SPECULAR : nrd::Denoiser::SIGMA_SHADOW};
            const nrd::InstanceCreationDesc info{.denoisers = &denoiser, .denoisersNum = 1};
            VKZ_ASSERT(nrd::CreateInstance(info, instance) == nrd::Result::SUCCESS, "NRD instance creation failed");
            description = nrd::GetInstanceDesc(*instance);
            offsets = nrd::GetLibraryDesc()->spirvBindingOffsets;
            if (shadow_light >= 0) {
                nrd::SigmaSettings settings{};
                VKZ_ASSERT(nrd::SetDenoiserSettings(*instance, 0, &settings) == nrd::Result::SUCCESS, "SIGMA settings failed");
                return;
            }

            nrd::RelaxSettings settings{};
            settings.enableAntiFirefly = true;
            settings.hitDistanceReconstructionMode = nrd::HitDistanceReconstructionMode::OFF;
            settings.diffusePrepassBlurRadius = 0.0f;
            settings.specularPrepassBlurRadius = 0.0f;
            settings.atrousIterationNum = 5;
            settings.lobeAngleFraction = 0.7f;
            settings.specularLobeAngleSlack = 0.2f;
            settings.depthThreshold = 0.004f;
            settings.diffuseMaxAccumulatedFrameNum = 25;
            settings.specularMaxAccumulatedFrameNum = 40;
            settings.diffuseMaxFastAccumulatedFrameNum = 5;
            settings.specularMaxFastAccumulatedFrameNum = 6;
            settings.antilagSettings.accelerationAmount = 0.55f;
            settings.antilagSettings.spatialSigmaScale = 2.5f;
            settings.antilagSettings.temporalSigmaScale = 0.3f;
            settings.antilagSettings.resetAmount = 0.5f;
            VKZ_ASSERT(nrd::SetDenoiserSettings(*instance, 0, &settings) == nrd::Result::SUCCESS, "NRD settings failed");
        }

        VkFormat format(nrd::Format value) {
            switch (value) {
            case nrd::Format::R8_UNORM:
                return VK_FORMAT_R8_UNORM;
            case nrd::Format::RG8_UNORM:
                return VK_FORMAT_R8G8_UNORM;
            case nrd::Format::R32_UINT:
                return VK_FORMAT_R32_UINT;
            case nrd::Format::R8_UINT:
                return VK_FORMAT_R8_UINT;
            case nrd::Format::R16_SFLOAT:
                return VK_FORMAT_R16_SFLOAT;
            case nrd::Format::R32_SFLOAT:
                return VK_FORMAT_R32_SFLOAT;
            case nrd::Format::RGBA8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case nrd::Format::RGBA16_SFLOAT:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            default:
                VKZ_THROW("Unsupported NRD texture format");
            }
        }

        vkz::texture create_texture(VkFormat format, uint32_t downsample = 1) {
            vkz::texture result;
            result.image = vkz::image::builder(allocator)
                .format(format)
                .extent((width + downsample - 1) / downsample, (height + downsample - 1) / downsample)
                .usage(vkz::image_usage_sampled_storage_download)
                .build();
            result.image_view = vkz::image_view::builder(device)
                .image(result.image)
                .format(format)
                .aspect_mask(vkz::image_aspect_color)
                .level_count(1)
                .layer_count(1)
                .build();
            return result;
        }

        void create_images() {
            for (auto &texture : images)
                texture = create_texture(VK_FORMAT_R32G32B32A32_SFLOAT);
            for (uint32_t i = 0; i < description->permanentPoolSize; ++i) {
                const auto &texture = description->permanentPool[i];
                permanent.push_back(create_texture(format(texture.format), texture.downsampleFactor));
            }
            for (uint32_t i = 0; i < description->transientPoolSize; ++i) {
                const auto &texture = description->transientPool[i];
                transient.push_back(create_texture(format(texture.format), texture.downsampleFactor));
            }
            constants = vkz::buffer::builder(allocator)
                .size(description->constantBufferMaxDataSize)
                .usage(vkz::buffer_usage_uniform_upload)
                .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                .build();
        }

        vkz::descriptor_set_layout create_layout(const std::vector<VkDescriptorSetLayoutBinding> &bindings) {
            vkz::descriptor_set_layout_builder builder{device};
            using binding_builder = vkz::descriptor_set_layout_builder::descriptor_set_layout_binding_builder;
            auto current = std::make_unique<binding_builder>(builder.binding(bindings.front().binding));
            for (size_t i = 0; i < bindings.size(); ++i) {
                const auto &binding = bindings[i];
                current->descriptor_type(static_cast<vkz::descriptor_type>(binding.descriptorType))
                    .descriptor_count(binding.descriptorCount)
                    .shader_stages(static_cast<vkz::shader_stage>(binding.stageFlags));
                if (binding.pImmutableSamplers)
                    current->immutable_sampler(vkz::sampler{.handle = *binding.pImmutableSamplers, .device = device});
                if (i + 1 < bindings.size())
                    current = std::make_unique<binding_builder>(current->binding(bindings[i + 1].binding));
            }
            return current->create_layout();
        }

        void create_layouts() {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            for (uint32_t i = 0; i < description->descriptorPoolDesc.perSetTexturesMaxNum; ++i)
                bindings.push_back(
                    {offsets.textureOffset + i, static_cast<VkDescriptorType>(vkz::descriptor_type_sampled_image), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_compute), nullptr});
            for (uint32_t i = 0; i < description->descriptorPoolDesc.perSetStorageTexturesMaxNum; ++i)
                bindings.push_back({offsets.storageTextureAndBufferOffset + i, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), 1,
                                    static_cast<VkShaderStageFlags>(vkz::shader_stage_compute), nullptr});
            layouts[0] = create_layout(bindings);

            bindings.clear();
            for (uint32_t i = 0; i < 2; ++i) {
                samplers[i] =
                    vkz::sampler::builder(device)
                        .min_filter(i ? VK_FILTER_LINEAR : VK_FILTER_NEAREST)
                        .mag_filter(i ? VK_FILTER_LINEAR : VK_FILTER_NEAREST)
                        .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
                        .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                    .build();
                bindings.push_back(
                    {offsets.samplerOffset + i, static_cast<VkDescriptorType>(vkz::descriptor_type_sampler), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_compute), &samplers[i].handle});
            }
            bindings.push_back(
                {offsets.constantBufferOffset, static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_compute), nullptr});
            layouts[1] = create_layout(bindings);

            bindings.clear();
            for (uint32_t i = 0; i < images.size(); ++i)
                bindings.push_back({i, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_compute), nullptr});
            bridge_layout = create_layout(bindings);
            descriptors = vkz::descriptor_pool{device,
                                               1024,
                                               {{static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), 8192},
                                                {static_cast<VkDescriptorType>(vkz::descriptor_type_sampled_image), 16384},
                                                {static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 2},
                                                {static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 6},
                                                {static_cast<VkDescriptorType>(vkz::descriptor_type_sampler), 2}}};
            constants_set = descriptors.allocate(layouts[1]);
            bridge_set = descriptors.allocate(bridge_layout);
            vkz::update_descriptor(device, {.descriptor_set = constants_set, .bindings = {vkz::descriptor<vkz::ubo_descriptor>(constants, offsets.constantBufferOffset)}});

            vkz::descriptor_bindings image_bindings{.descriptor_set = bridge_set};
            for (uint32_t i = 0; i < images.size(); ++i)
                image_bindings.bindings.push_back(vkz::image_descriptor{images[i].image_view, VK_IMAGE_LAYOUT_GENERAL, i});
            vkz::update_descriptor(device, std::move(image_bindings));
        }

        void create_input_descriptors(const input &inputs) {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            for (uint32_t i = 0; i < 6; ++i)
                bindings.push_back({i, static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_compute), nullptr});
            for (uint32_t i = 6; i < 9; ++i)
                bindings.push_back({i, static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_compute), nullptr});
            bindings.push_back({9, static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 1, static_cast<VkShaderStageFlags>(vkz::shader_stage_compute), nullptr});
            input_layout = create_layout(bindings);
            input_set = descriptors.allocate(input_layout);
            input_sampler =
                vkz::sampler::builder(device)
                    .min_filter(VK_FILTER_NEAREST)
                    .mag_filter(VK_FILTER_NEAREST)
                    .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
                    .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                .build();
            frame_buffer = vkz::buffer::builder(allocator)
                .size(sizeof(frame_data))
                .usage(vkz::buffer_usage_uniform_upload)
                .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                .build();
            update_input_descriptors(inputs);
        }

        void update_input_descriptors(const input &inputs) {
            const std::array<const vkz::texture *, 6> textures{&inputs.depth, &inputs.normal, &inputs.surface,
                &inputs.diffuse, &inputs.specular, &inputs.penumbra};
            vkz::descriptor_bindings bindings{.descriptor_set = input_set};
            for (uint32_t i = 0; i < textures.size(); ++i)
                bindings.bindings.push_back(vkz::texture_descriptor{textures[i]->image_view, input_sampler,
                    i < 3 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL, i});
            bindings.bindings.push_back(vkz::descriptor<vkz::image_descriptor>(inputs.diffuse_output, 6));
            bindings.bindings.push_back(vkz::descriptor<vkz::image_descriptor>(inputs.specular_output, 7));
            bindings.bindings.push_back(vkz::descriptor<vkz::image_descriptor>(inputs.shadow_output, 8));
            bindings.bindings.push_back(vkz::descriptor<vkz::ubo_descriptor>(frame_buffer, 9));
            vkz::update_descriptor(device, std::move(bindings));
        }

        void create_pipelines() {
            for (uint32_t i = 0; i < description->pipelinesNum; ++i) {
                const auto &shader = description->pipelines[i].computeShaderSPIRV;
                const auto *begin = static_cast<const uint32_t *>(shader.bytecode);
                std::vector<uint32_t> words(begin, begin + shader.size / sizeof(uint32_t));
                pipelines.push_back(
                    vkz::compute_pipeline_builder{device}
                        .shader_stage()
                            .compute_shader(words)
                            .entry_point(description->shaderEntryPoint)
                        .layout()
                            .add_descriptor_set_layouts(layouts)
                        .name("nrd_pipeline_" + std::to_string(i))
                    .build());
            }
            const char *names[] = {"nrd_prepare.comp.spv", "nrd_resolve.comp.spv"};
            for (uint32_t i = 0; i < 2; ++i) {
                const auto file = cmrc::rtx_shaders::get_filesystem().open(names[i]);
                std::vector<uint32_t> shader(file.size() / 4);
                std::memcpy(shader.data(), file.begin(), file.size());
                bridge_pipelines[i] =
                    vkz::compute_pipeline_builder{device}
                        .shader_stage()
                            .compute_shader(shader)
                        .layout()
                            .add_descriptor_set_layout(bridge_layout)
                            .add_descriptor_set_layout(input_layout)
                        .name(names[i])
                    .build();
            }
        }

        vkz::texture &resource(const nrd::ResourceDesc &resource) {
            switch (resource.type) {
            case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:
                return images[0];
            case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:
                return images[1];
            case nrd::ResourceType::IN_NORMAL_ROUGHNESS:
                return images[2];
            case nrd::ResourceType::IN_VIEWZ:
                return images[3];
            case nrd::ResourceType::IN_PENUMBRA:
                return images[5];
            case nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY:
                return images[11];
            case nrd::ResourceType::IN_MV:
                return images[4];
            case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:
                return images[10];
            case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:
                return images[11];
            case nrd::ResourceType::PERMANENT_POOL:
                return permanent[resource.indexInPool];
            case nrd::ResourceType::TRANSIENT_POOL:
                return transient[resource.indexInPool];
            default:
                VKZ_THROW("Unsupported NRD resource");
            }
        }

        vkz::descriptor_set dispatch_set(const nrd::DispatchDesc &dispatch) {
            std::vector<VkImageView> key;
            for (uint32_t i = 0; i < dispatch.resourcesNum; ++i)
                key.push_back(resource(dispatch.resources[i]).image_view);
            const auto cache_key = std::make_pair(dispatch.pipelineIndex, key);
            if (auto found = cached_sets.find(cache_key); found != cached_sets.end()) return found->second;
            auto set = descriptors.allocate(layouts[0]);
            uint32_t sampled = offsets.textureOffset;
            uint32_t storage = offsets.storageTextureAndBufferOffset;
            // NRD uses separate sampled images; descriptor_t currently only models combined samplers.
            for (uint32_t i = 0; i < dispatch.resourcesNum; ++i) {
                bool writable = dispatch.resources[i].descriptorType == nrd::DescriptorType::STORAGE_TEXTURE;
                VkDescriptorImageInfo image{.imageView = key[i], .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
                VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                           .dstSet = set.handle,
                                           .dstBinding = writable ? storage++ : sampled++,
                                           .descriptorCount = 1,
                                           .descriptorType =
                                               writable ? static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image) : static_cast<VkDescriptorType>(vkz::descriptor_type_sampled_image),
                                           .pImageInfo = &image};
                vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
            }
            cached_sets.emplace(cache_key, set);
            return set;
        }

        void transition(VkCommandBuffer command_buffer, vkz::texture &texture) {
            const VkImageSubresourceRange range{.aspectMask = static_cast<VkImageAspectFlags>(vkz::image_aspect_color), .levelCount = 1, .layerCount = 1};
            vkz::barrier::push_and_flush(command_buffer, texture.image, range, vkz::pipeline_stage_all_commands, vkz::pipeline_stage_all_commands, vkz::access_memory_read_write,
                                         vkz::access_memory_read_write, VK_IMAGE_LAYOUT_GENERAL);
        }

        void prepare(VkCommandBuffer command_buffer) {
            for (auto &image : images)
                transition(command_buffer, image);
        }

        void bridge(VkCommandBuffer command_buffer, uint32_t index) {
            for (auto &image : images)
                transition(command_buffer, image);

            vkz::bind_pipeline(command_buffer, bridge_pipelines[index]);
            vkz::bind_descriptor_sets(command_buffer, bridge_pipelines[index], {bridge_set, input_set});
            vkz::dispatch(command_buffer, (width + 7) / 8, (height + 7) / 8);
        }

        void resolve(VkCommandBuffer command_buffer, const vkz::camera::camera &camera, const input &inputs, bool reset) {
            update_input_descriptors(inputs);
            frame_data frame{glm::inverse(camera.projection * camera.view), camera.view,
                camera.position, shadow_light};
            synchronize(command_buffer);
            vkz::update(command_buffer, frame_buffer, frame);
            synchronize(command_buffer);
            for (auto *output : {&inputs.diffuse_output, &inputs.specular_output, &inputs.shadow_output})
                transition(command_buffer, *output);
            profile_pass(command_buffer, "Prepare inputs", [&] { bridge(command_buffer, 0); });
            nrd::CommonSettings settings{};
            auto projection = camera.projection;
            projection[1][1] *= -1;
            std::memcpy(settings.worldToViewMatrix, glm::value_ptr(camera.view), sizeof(glm::mat4));
            std::memcpy(settings.viewToClipMatrix, glm::value_ptr(projection), sizeof(glm::mat4));
            std::memcpy(settings.worldToViewMatrixPrev, glm::value_ptr(have_history ? previous_view : camera.view), sizeof(glm::mat4));
            std::memcpy(settings.viewToClipMatrixPrev, glm::value_ptr(have_history ? previous_projection : projection), sizeof(glm::mat4));
            settings.resourceSize[0] = settings.resourceSizePrev[0] = settings.rectSize[0] = settings.rectSizePrev[0] = uint16_t(width);
            settings.resourceSize[1] = settings.resourceSizePrev[1] = settings.rectSize[1] = settings.rectSizePrev[1] = uint16_t(height);
            settings.isMotionVectorInWorldSpace = true;
            settings.motionVectorScale[2] = 1.0f;
            settings.denoisingRange = camera.zfar;
            settings.frameIndex = frame_index++;
            settings.accumulationMode = !have_history || reset ? nrd::AccumulationMode::CLEAR_AND_RESTART : nrd::AccumulationMode::CONTINUE;
            VKZ_ASSERT(nrd::SetCommonSettings(*instance, settings) == nrd::Result::SUCCESS, "NRD common settings failed");
            const nrd::Identifier identifier = 0;
            const nrd::DispatchDesc *dispatches;
            uint32_t count;
            VKZ_ASSERT(nrd::GetComputeDispatches(*instance, &identifier, 1, dispatches, count) == nrd::Result::SUCCESS, "NRD dispatch generation failed");
            for (uint32_t i = 0; i < count; ++i) {
                const auto &dispatch = dispatches[i];
                profile_pass(command_buffer, dispatch.name, [&] {
                    for (uint32_t j = 0; j < dispatch.resourcesNum; ++j)
                        transition(command_buffer, resource(dispatch.resources[j]));
                    if (dispatch.constantBufferDataSize) {
                        vkz::barrier::push_and_flush(command_buffer, vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_transfer, vkz::access_uniform_read, vkz::access_transfer_write);
                        vkz::update(command_buffer, constants, dispatch.constantBufferDataSize, const_cast<uint8_t *>(dispatch.constantBufferData));
                        vkz::barrier::push_and_flush(command_buffer, vkz::pipeline_stage_transfer, vkz::pipeline_stage_compute_shader, vkz::access_transfer_write, vkz::access_uniform_read);
                    }
                    auto &pipeline = pipelines[dispatch.pipelineIndex];
                    vkz::bind_pipeline(command_buffer, pipeline);
                    vkz::bind_descriptor_sets(command_buffer, pipeline, {dispatch_set(dispatch), constants_set});
                    vkz::dispatch(command_buffer, dispatch.gridWidth, dispatch.gridHeight);
                });
            }
            profile_pass(command_buffer, "Resolve output", [&] { bridge(command_buffer, 1); });
            previous_view = camera.view;
            previous_projection = projection;
            have_history = true;
        }

        vkz::device device;
        vkz::vma_memory_allocator allocator;
        uint32_t width;
        uint32_t height;
        struct frame_data {
            glm::mat4 inverse_view_projection;
            glm::mat4 view;
            glm::vec3 camera_position;
            int shadow_light;
        };
        static_assert(sizeof(frame_data) == 144);
        int shadow_light;
        vkz::descriptor_set_layout input_layout;
        vkz::descriptor_set input_set;
        vkz::buffer frame_buffer;
        vkz::sampler input_sampler;
        nrd::Instance *instance{};
        const nrd::InstanceDesc *description{};
        nrd::SPIRVBindingOffsets offsets{};
        std::array<vkz::texture, 12> images;
        std::vector<vkz::texture> permanent;
        std::vector<vkz::texture> transient;
        std::array<vkz::sampler, 2> samplers;
        std::array<vkz::descriptor_set_layout, 2> layouts;
        vkz::descriptor_set_layout bridge_layout;
        vkz::descriptor_pool descriptors;
        vkz::descriptor_set constants_set;
        vkz::descriptor_set bridge_set;
        vkz::buffer constants;
        std::vector<vkz::pipeline> pipelines;
        std::array<vkz::pipeline, 2> bridge_pipelines;
        std::map<std::pair<uint16_t, std::vector<VkImageView>>, vkz::descriptor_set> cached_sets;
        glm::mat4 previous_view{1};
        glm::mat4 previous_projection{1};
        glm::vec2 previous_jitter{};
        bool have_history{};
        uint32_t frame_index{};
    };

    nrd_denoiser::nrd_denoiser(vkz::context &context, glm::uvec2 resolution, const input &inputs, int shadow_light)
        : state_(std::make_unique<implementation>(context, resolution, inputs, shadow_light)) {}

    nrd_denoiser::~nrd_denoiser() = default;

    void nrd_denoiser::exec(VkCommandBuffer cmd, const vkz::camera::camera &camera, const input &inputs, bool reset) {
        state_->resolve(cmd, camera, inputs, reset);
    }
}
