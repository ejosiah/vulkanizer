#include <vulkanizer/ibl.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/builders.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/descriptors.hpp>
#include <vulkanizer/ktx.hpp>
#include <vulkanizer/mesh.hpp>
#include <cmrc/cmrc.hpp>
#include <algorithm>
#include <bit>
#include <cstring>

CMRC_DECLARE(vkz_ibl);

namespace vkz::ibl {
    namespace {
        std::vector<uint32_t> shader(const char *name) {
            const auto file = cmrc::vkz_ibl::get_filesystem().open(name);
            VKZ_ASSERT(file.size() % sizeof(uint32_t) == 0, "Invalid IBL SPIR-V");
            std::vector<uint32_t> words(file.size() / sizeof(uint32_t));
            std::memcpy(words.data(), file.begin(), file.size());
            return words;
        }

        void validate_output(const texture &output) {
            const auto &info = output.image.create_info;
            VKZ_ASSERT(output.image.handle && output.image_view.handle && info.format == VK_FORMAT_R32G32B32A32_SFLOAT && info.arrayLayers == 6 && (info.flags & static_cast<VkImageCreateFlags>(vkz::image::flags::cube_compatible)) &&
                           (info.usage & static_cast<VkImageUsageFlags>(vkz::image::usage::storage)),
                       "IBL output must be a six-layer RGBA32F storage cubemap");
        }

        void transition(VkCommandBuffer cmd, texture &output, VkImageLayout layout) {
            barrier::push_and_flush(cmd, output.image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, output.image.create_info.mipLevels, 0, 6},
                                    output.image.layout == VK_IMAGE_LAYOUT_UNDEFINED ? vkz::pipeline::stage::none : vkz::pipeline::stage::all_commands, vkz::pipeline::stage::all_commands,
                                    output.image.layout == VK_IMAGE_LAYOUT_UNDEFINED ? vkz::access::none : vkz::access::memory_read_write, vkz::access::memory_read_write, layout);
        }

        texture make_cube(vma_memory_allocator &allocator, uint32_t size, uint32_t levels) {
            texture result;
            try {
                result.image = image::builder(allocator)
                    .flags(vkz::image::flags::cube_compatible)
                    .format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .extent(size, size)
                    .array_layers(6)
                    .mip_levels(levels)
                    .usage(vkz::image::usage::sampled_storage_download)
                    .build();
                result.image_view = image_view::builder(allocator.device)
                    .image(result.image)
                    .view_type(VK_IMAGE_VIEW_TYPE_CUBE)
                    .format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .aspect_mask(vkz::image::aspect::color)
                    .layer_count(6)
                    .level_count(levels)
                    .build();
                result.sampler = sampler::builder(allocator.device)
                    .mag_filter(VK_FILTER_LINEAR)
                    .min_filter(VK_FILTER_LINEAR)
                    .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                    .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                    .max_lod(float(levels - 1))
                    .build();
            } catch (...) {
                result.destroy();
                throw;
            }
            return result;
        }
    }

    struct generator::implementation {
        ~implementation() {
            equirectangular_pipeline.destroy();
            irradiance_pipeline.destroy();
            specular_pipeline.destroy();
            pool.destroy();
            layout.destroy();
            for (auto &view : views)
                view.destroy();
        }

        void init(vkz::device value) {
            device = value;
            create_descriptors();
            create_pipelines();
        }

        void create_descriptors() {
            pool = descriptor_pool{
                device, 1024, {{static_cast<VkDescriptorType>(vkz::descriptor::type::combined_image_sampler), 1024}, {static_cast<VkDescriptorType>(vkz::descriptor::type::storage_image), 1024}}};
            layout = make_descriptor_set_layout_builder(device)
                         .binding(0)
                         .descriptor_type(vkz::descriptor::type::combined_image_sampler)
                         .descriptor_count(1)
                         .shader_stages(vkz::shader_stage::compute)
                         .binding(1)
                         .descriptor_type(vkz::descriptor::type::storage_image)
                         .descriptor_count(1)
                         .shader_stages(vkz::shader_stage::compute)
                         .create_layout();
        }

        pipeline create_pipeline(const char *name, bool roughness) {
            auto builder = make_compute_pipeline_builder(device);
            builder
                .shader_stage()
                    .compute_shader(shader(name))
                .layout()
                    .add_descriptor_set_layout(layout);
            if (roughness) {
                builder.layout().add_push_constant_range(vkz::shader_stage::compute, 0, sizeof(float));
            }
            return builder.build();
        }

        void create_pipelines() {
            equirectangular_pipeline = create_pipeline("equirectangular.comp.spv", false);
            irradiance_pipeline = create_pipeline("irradiance.comp.spv", false);
            specular_pipeline = create_pipeline("specular.comp.spv", true);
        }

        descriptor_set make_set(const texture &source, const texture &output, uint32_t mip) {
            for (const auto &binding : bindings) {
                if (binding.source == source.image_view.handle && binding.output == output.image.handle && binding.mip == mip) return binding.set;
            }
            auto view = image_view::builder(device)
                .image(output.image)
                .view_type(VK_IMAGE_VIEW_TYPE_CUBE)
                .format(output.image.create_info.format)
                .aspect_mask(vkz::image::aspect::color)
                .base_mip_level(mip)
                .level_count(1)
                .layer_count(6)
                .build();
            views.push_back(view);
            auto set = pool.allocate(layout);
            auto sampled = source;
            update_descriptors(device, {{set, {make_descriptor<texture_descriptor>(sampled, 0), image_descriptor{view, VK_IMAGE_LAYOUT_GENERAL, 1}}}});
            bindings.push_back({source.image_view.handle, output.image.handle, mip, set});
            return set;
        }

        void validate(VkCommandBuffer cmd, const texture &source, const texture &output) {
            VKZ_ASSERT(cmd && source.image_view.handle && source.sampler.handle, "IBL requires a command buffer and sampled input");
            VKZ_ASSERT(source.image_view.device == device.logical && output.image_view.device == device.logical, "IBL textures must belong to the generator device");
            VKZ_ASSERT(source.image.handle != output.image.handle, "IBL input and output must be distinct images");
            validate_output(output);
        }

        void dispatch_map(VkCommandBuffer cmd, pipeline &pipeline, const texture &source, const texture &output, uint32_t mip) {
            pipeline.descriptor_sets = {make_set(source, output, mip)};
            bind_pipeline(cmd, pipeline);
            const auto extent = output.image.create_info.extent;
            dispatch(cmd, (std::max(1u, extent.width >> mip) + 7) / 8, (std::max(1u, extent.height >> mip) + 7) / 8, 6);
        }

        struct generation_binding {
            VkImageView source{};
            VkImage output{};
            uint32_t mip{};
            descriptor_set set;
        };

        vkz::device device;
        descriptor_pool pool;
        descriptor_set_layout layout;
        pipeline equirectangular_pipeline;
        pipeline irradiance_pipeline;
        pipeline specular_pipeline;
        std::vector<image_view> views;
        std::vector<generation_binding> bindings;
    };

    generator::generator(vkz::device device) : state(std::make_unique<implementation>()) {
        state->init(device);
    }

    generator::~generator() = default;

    void generator::convert(VkCommandBuffer cmd, const texture &source, texture &environment) {
        state->validate(cmd, source, environment);
        VKZ_ASSERT(environment.image.create_info.mipLevels == 1, "Environment conversion requires one output mip");
        transition(cmd, environment, VK_IMAGE_LAYOUT_GENERAL);
        state->dispatch_map(cmd, state->equirectangular_pipeline, source, environment, 0);
        transition(cmd, environment, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void generator::generate(VkCommandBuffer cmd, const texture &environment, texture &irradiance, texture &specular, VkImageLayout final_layout) {
        state->validate(cmd, environment, irradiance);
        state->validate(cmd, environment, specular);
        VKZ_ASSERT(irradiance.image.handle != specular.image.handle && irradiance.image.create_info.mipLevels == 1, "IBL requires distinct outputs and a single irradiance mip");
        transition(cmd, irradiance, VK_IMAGE_LAYOUT_GENERAL);
        transition(cmd, specular, VK_IMAGE_LAYOUT_GENERAL);
        state->dispatch_map(cmd, state->irradiance_pipeline, environment, irradiance, 0);
        const auto levels = specular.image.create_info.mipLevels;
        for (uint32_t mip = 0; mip < levels; ++mip) {
            const float roughness = levels > 1 ? float(mip) / float(levels - 1) : 0;
            push_constants(cmd, state->specular_pipeline.layout, roughness, vkz::shader_stage::compute);
            state->dispatch_map(cmd, state->specular_pipeline, environment, specular, mip);
        }
        transition(cmd, irradiance, final_layout);
        transition(cmd, specular, final_layout);
    }

    texture load_brdf_lut(vma_memory_allocator &allocator, VkQueue queue, uint32_t queue_family) {
        const auto file = cmrc::vkz_ibl::get_filesystem().open("brdf_lut.ktx");
        mesh::sampler settings;
        settings.wrap_s = settings.wrap_t = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        return texture_from_ktx({reinterpret_cast<const uint8_t *>(file.begin()), file.size()}, allocator, queue, queue_family, settings);
    }

    maps load(vma_memory_allocator &allocator, VkQueue queue, uint32_t queue_family, const std::filesystem::path &file, const settings &options) {
        VKZ_ASSERT(queue && options.environment_size && options.irradiance_size && options.specular_size, "IBL requires a queue and nonzero map sizes");
        maps result;
        texture source;
        try {
            source = vkz::load(allocator, queue, queue_family, file, VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
            source.sampler.destroy();
            source.sampler = sampler::builder(allocator.device)
                .mag_filter(VK_FILTER_LINEAR)
                .min_filter(VK_FILTER_LINEAR)
                .address_mode_u(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                .address_mode_v(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                .max_lod(0)
                .build();
            result.environment = make_cube(allocator, options.environment_size, 1);
            result.irradiance = make_cube(allocator, options.irradiance_size, 1);
            result.specular = make_cube(allocator, options.specular_size, std::bit_width(options.specular_size));
            result.brdf_lut = load_brdf_lut(allocator, queue, queue_family);
            generator generation{allocator.device};
            command_pool commands{allocator.device, queue_family, vkz::command_pool_flags::transient, queue};
            const auto cmd = commands.create_command_buffer();
            generation.convert(cmd, source, result.environment);
            generation.generate(cmd, result.environment, result.irradiance, result.specular);
            commands.submit_and_wait(cmd);
        } catch (...) {
            source.destroy();
            result.destroy();
            throw;
        }
        source.destroy();
        return result;
    }

    void maps::destroy() {
        environment.destroy();
        irradiance.destroy();
        specular.destroy();
        brdf_lut.destroy();
    }
}
