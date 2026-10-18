#include "rtx_nrc.hpp"
#include "rtx_support.hpp"
#include <vulkanizer/ray_tracing_pipeline_builder.hpp>

namespace rtx {
    nrc_gi::nrc_gi(vkz::context &context, vkz::vma_memory_allocator &allocator,
        const vkz::reflection &reflection, uint32_t texture_count, glm::uvec2 resolution,
        glm::vec4 bounds, VkQueue queue, uint32_t queue_family)
        : cache_(context, resolution.x, resolution.y), tracing_(reflection.tracing_resources()), resolution_(resolution) {
        cache_.configure(bounds, 4, true);
        indirect_light_.image = vkz::image::builder(allocator)
            .format(VK_FORMAT_R32G32B32A32_SFLOAT)
            .extent(resolution.x, resolution.y)
            .usage(vkz::image_usage_sampled_storage_transfer)
            .build();
        indirect_light_.image_view =
            vkz::image_view::builder(context.device)
                .image(indirect_light_.image)
                .format(VK_FORMAT_R32G32B32A32_SFLOAT)
                .aspect_mask(vkz::image_aspect_color)
                .level_count(1)
                .layer_count(1)
                .build();
        indirect_light_.sampler =
            vkz::sampler::builder(context.device)
                .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
            .build();

        pipeline_ = vkz::ray_tracing_pipeline_builder{allocator}
                        .shader_groups()
                        .ray_generation_group("nrc")
                        .ray_generation_shader(load_shader("nrc.rgen.spv"))
                        .miss_group("miss")
                        .miss_shader(load_shader("nrc.rmiss.spv"))
                        .triangles_hit_group("surface")
                        .closest_hit_shader(load_shader("nrc.rchit.spv"))
                        .add_specialization(texture_count, 0)
                        .any_hit_shader(load_shader("nrc.rahit.spv"))
                        .add_specialization(texture_count, 0)
                        .layout()
                        .add_descriptor_set_layouts(tracing_.layouts)
                        .add_descriptor_set_layout(cache_.layout())
                        .add_push_constant_range(vkz::shader_stage_raygen, 0, sizeof(uint32_t))
                        .max_recursion_depth(1)
                        .name("nrc_raster_bounce")
                        .build();

        output_layout_ = vkz::descriptor_set_layout_builder{context.device}
                             .binding(0)
                             .descriptor_type(vkz::descriptor_type_storage_image)
                             .descriptor_count(1)
                             .shader_stages(vkz::shader_stage_compute)
                             .create_layout();
        output_pool_ = vkz::descriptor_pool{context.device, 1, {{static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), 1}}};
        output_set_ = output_pool_.allocate(output_layout_);
        vkz::update_descriptor(context.device, {.descriptor_set = output_set_,
            .bindings = {vkz::image_descriptor{indirect_light_.image_view, VK_IMAGE_LAYOUT_GENERAL, 0}}});
        resolve_pipeline_ =
            vkz::compute_pipeline_builder{context.device}
                .shader_stage()
                    .compute_shader(load_shader("nrc_resolve.comp.spv"))
                .layout()
                    .add_descriptor_set_layout(cache_.layout())
                    .add_descriptor_set_layout(output_layout_)
                .name("nrc_radiance_resolve")
            .build();

        vkz::command_pool uploads{context.device, queue_family, vkz::command_pool_flags_transient, queue};
        const auto cmd = uploads.create_command_buffer();
        pipeline_.sbt.upload(cmd);
        vkz::clear(cmd, indirect_light_.image, VkClearColorValue{}, {});
        vkz::barrier::push_and_flush(cmd, indirect_light_.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_transfer,
                                     vkz::pipeline_stage_all_commands, vkz::access_transfer_write, vkz::access_shader_read_write, VK_IMAGE_LAYOUT_GENERAL);
        uploads.submit_and_wait(cmd);
    }

    nrc_gi::~nrc_gi() {
        resolve_pipeline_.destroy();
        output_pool_.destroy();
        output_layout_.destroy();
        pipeline_.destroy();
        indirect_light_.destroy();
    }

    void nrc_gi::exec(VkCommandBuffer cmd) {
        // NRC's normal resolve adds its query contribution to the supplied prefix.
        // Our raster GI prefix is zero every frame, rather than accumulated output.
        vkz::clear(cmd, indirect_light_.image, VkClearColorValue{}, {});
        cache_.begin_frame(cmd);
        vkz::bind_pipeline(cmd, pipeline_);
        vkz::bind_descriptor_sets(cmd, pipeline_, {tracing_.sets[0], tracing_.sets[1], cache_.bindings()});
        vkz::push_constants(cmd, pipeline_, 1u, vkz::shader_stage_raygen);
        const auto training = cache_.training_dimensions();
        vkz::trace_ray(cmd, pipeline_, training.x, training.y, "nrc");
        synchronize(cmd);
        vkz::push_constants(cmd, pipeline_, 0u, vkz::shader_stage_raygen);
        vkz::trace_ray(cmd, pipeline_, resolution_.x, resolution_.y, "nrc");
        cache_.resolve(cmd, indirect_light_.image, indirect_light_.image_view);
        synchronize(cmd);
        vkz::bind_pipeline(cmd, resolve_pipeline_);
        vkz::bind_descriptor_sets(cmd, resolve_pipeline_, {cache_.bindings(), output_set_});
        vkz::dispatch(cmd, (resolution_.x + 7) / 8, (resolution_.y + 7) / 8);
        synchronize(cmd);
    }

    void nrc_gi::end_frame(VkQueue queue) {
        cache_.end_frame(queue);
    }

    void nrc_gi::reset(glm::vec4 bounds) {
        cache_.configure(bounds, 4, true);
    }
}
