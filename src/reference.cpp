#include <vulkanizer/reference.hpp>
#include <vulkanizer/graphics_pipeline_builder.hpp>
#include <cmrc/cmrc.hpp>
#include <cstring>
#include <stdexcept>
#include <vulkanizer/status.hpp>

CMRC_DECLARE(vkz_reference_shaders);

namespace vkz::reference {
    std::vector<uint32_t> shader(shader_stage stage, bool ibl, bool ray_query) {
        static constexpr std::array names{"gltf_reference.vert.spv",  "gltf_reference.frag.spv",  "gltf_reference.rgen.spv",  "gltf_reference.rmiss.spv", "gltf_reference.rchit.spv",
                                          "gltf_reference.rahit.spv", "gltf_background.vert.spv", "gltf_background.frag.spv", "tone_mapping.frag.spv"};
        VKZ_ASSERT(static_cast<size_t>(stage) < names.size(), "Invalid reference shader stage");
        std::string name = names[static_cast<size_t>(stage)];
        if (ibl && (stage == shader_stage::fragment || stage == shader_stage::miss || stage == shader_stage::closest_hit)) name += ".ibl";
        if (ray_query && stage == shader_stage::fragment) name += ".query";
        const auto file = cmrc::vkz_reference_shaders::get_filesystem().open(name);
        VKZ_ASSERT(file.size() % sizeof(uint32_t) == 0, "Invalid embedded SPIR-V");
        std::vector<uint32_t> words(file.size() / 4);
        std::memcpy(words.data(), file.begin(), file.size());
        return words;
    }

    vkz::pipeline create_pipeline(const pipeline_create_info &info) {
        if (!info.device.logical || !info.extent.width || !info.extent.height || !info.layouts[0] || !info.layouts[1] || info.color_format == VK_FORMAT_UNDEFINED ||
            info.depth_format == VK_FORMAT_UNDEFINED) {
            throw std::invalid_argument("Reference pipeline requires a device, extent, formats and descriptor layouts");
        }
        vkz::graphics_pipeline_builder builder{info.device};
        builder
            .shader_stage()
                .vertex_shader(shader(shader_stage::vertex))
                .fragment_shader(shader(shader_stage::fragment, bool(info.ibl_layout), bool(info.ray_query_layout)))
                    .add_specialization(info.texture_count, 0)
            .input_assembly_state()
                .triangles()
            .viewport_state()
                .viewport()
                    .origin(0, 0)
                    .dimension(info.extent.width, info.extent.height)
                    .min_depth(0)
                    .max_depth(1)
                .scissor()
                    .offset(0, 0)
                    .extent(info.extent.width, info.extent.height)
                    .add()
            .rasterization_state()
                .cull_none()
                .polygon_mode_fill()
            .multisample_state()
                .rasterization_samples(info.samples)
            .depth_stencil_state()
                .enable_depth_test()
                .compare_op_less_or_equal();

        if (info.mirrored) {
            builder
                .rasterization_state()
                    .front_face_clockwise();
        } else {
            builder
                .rasterization_state()
                    .front_face_counter_clockwise();
        }

        if (info.alpha_blend) {
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
            builder
                .depth_stencil_state()
                    .enable_depth_write();
        }

        builder.color_blend_state()
            .attachment()
            .add()
            .layout()
            .add_descriptor_set_layouts(info.layouts)
            .add_push_constant_range(vkz::shader_stage::vertex | vkz::shader_stage::fragment, 0, sizeof(instance_uniform));
        if (info.ibl_layout) {
            builder.layout().add_descriptor_set_layout(info.ibl_layout);
        }
        if (info.ray_query_layout) {
            builder.layout().add_descriptor_set_layout(info.ray_query_layout);
        }
        return builder
                .dynamic_render_pass()
                    .add_color_attachment(info.color_format)
                    .depth_attachment(info.depth_format)
                .name("gltf_core_reference")
                .build();
    }

    std::array<VkDescriptorSetLayoutBinding, 5> ibl_bindings(vkz::shader_stage stages) {
        std::array<VkDescriptorSetLayoutBinding, 5> result{};
        for (uint32_t i = 0; i < result.size(); ++i)
            result[i] = {i, i == 4 ? static_cast<VkDescriptorType>(vkz::descriptor::type::uniform_buffer) : static_cast<VkDescriptorType>(vkz::descriptor::type::combined_image_sampler), 1,
                         static_cast<VkShaderStageFlags>(stages), nullptr};
        return result;
    }

    vkz::pipeline create_background_pipeline(const background_pipeline_create_info &info) {
        VKZ_ASSERT(info.device.logical && info.ibl_layout && info.extent.width && info.extent.height, "Reference background requires a device, IBL layout and extent");
        return graphics_pipeline_builder{info.device}
            .shader_stage()
                .vertex_shader(shader(shader_stage::background_vertex))
                .fragment_shader(shader(shader_stage::background_fragment))
            .input_assembly_state()
                .triangles()
            .viewport_state()
                .viewport()
                    .origin(0, 0)
                    .dimension(info.extent.width, info.extent.height)
                    .min_depth(0)
                    .max_depth(1)
                .scissor()
                    .offset(0, 0)
                    .extent(info.extent.width, info.extent.height)
                    .add()
            .rasterization_state()
                .cull_none()
                .polygon_mode_fill()
            .multisample_state()
                .rasterization_samples(info.samples)
            .depth_stencil_state()
                .enable_depth_test()
                .disable_depth_write()
                .compare_op_less_or_equal()
            .color_blend_state()
                .attachment()
                    .add()
            .layout()
                .add_descriptor_set_layout(info.ibl_layout)
            .dynamic_render_pass()
                .add_color_attachment(info.color_format)
                .depth_attachment(info.depth_format)
            .name("gltf_reference_environment")
            .build();
    }

    vkz::pipeline create_tone_mapping_pipeline(const tone_mapping_pipeline_create_info &info) {
        VKZ_ASSERT(info.device.logical && info.layout && info.extent.width && info.extent.height && info.color_format != VK_FORMAT_UNDEFINED,
                   "Tone mapping requires a device, input layout, extent and output format");
        return graphics_pipeline_builder{info.device}
            .shader_stage()
            .vertex_shader(shader(shader_stage::background_vertex))
            .fragment_shader(shader(shader_stage::tone_mapping_fragment))
            .input_assembly_state()
            .triangles()
            .viewport_state()
            .viewport()
            .origin(0, 0)
            .dimension(info.extent.width, info.extent.height)
            .min_depth(0)
            .max_depth(1)
            .scissor()
            .offset(0, 0)
            .extent(info.extent.width, info.extent.height)
            .add()
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
            .add_descriptor_set_layout(info.layout)
            .add_push_constant_range(vkz::shader_stage::fragment, 0, sizeof(tone_mapping_uniform))
            .dynamic_render_pass()
            .add_color_attachment(info.color_format)
            .name("reference_tone_mapping")
            .build();
    }

}
