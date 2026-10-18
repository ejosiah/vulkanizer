#define VKZ_IOSTREAM_ADAPTER
#include "../src/third_party/tinyobjloader/tiny_obj_loader.h"
#include "svsm_validation.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "timeout.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <limits>
#include <stb_image_write.h>
#include <unordered_map>
#include <vulkanizer/application.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/builders.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/csm.hpp>
#include <vulkanizer/glfw_input_adaptor.hpp>
#include <vulkanizer/graphics_pipeline_builder.hpp>
#include <vulkanizer/imgui.hpp>
#include <vulkanizer/ktx.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/mip_map.hpp>
#include <vulkanizer/render.hpp>
#include <vulkanizer/svsm.hpp>
#include <vulkanizer/vulkan_app.hpp>

namespace {
    constexpr uint32_t width = 1280, height = 800;
    struct vertex {
        glm::vec3 position, normal;
        glm::vec2 uv;
    };
    struct batch {
        uint32_t first, count;
        int material;
        vkz::svsm::caster_bounds bounds;
    };
    struct scene_constants {
        glm::mat4 matrix{1};
        glm::vec4 diffuse{1}, light_direction{};
        glm::uvec4 info{};
    };
    static_assert(sizeof(scene_constants) == 112);
    struct model {
        std::vector<vertex> vertices;
        std::vector<batch> batches;
        std::vector<tinyobj::material_t> materials;
        glm::vec3 minimum{std::numeric_limits<float>::max()}, maximum{-std::numeric_limits<float>::max()};
    };
    model load_model(const std::filesystem::path &path, float scale) {
        tinyobj::ObjReader reader;
        tinyobj::ObjReaderConfig config;
        config.triangulate = true;
        config.vertex_color = false;
        config.mtl_search_path = path.parent_path().string();
        std::cout << "Loading " << path << " with tinyobjloader..." << std::endl;
        if (!reader.ParseFromFile(path.string(), config))
            throw std::runtime_error(reader.Error());
        if (!reader.Warning().empty())
            std::cerr << reader.Warning() << '\n';
        model result;
        result.materials = reader.GetMaterials();
        const auto &a = reader.GetAttrib();
        for (const auto &shape : reader.GetShapes()) {
            batch current{};
            bool open = false;
            glm::vec3 lo{}, hi{};
            auto flush = [&] {
                if (!open)
                    return;
                current.bounds.center = glm::vec4((lo + hi) * 0.5f, 0);
                current.bounds.half_extent = glm::vec4((hi - lo) * 0.5f, 0);
                current.bounds.metadata.x = uint32_t(result.batches.size());
                result.batches.push_back(current);
                open = false;
            };
            std::vector<size_t> faces(shape.mesh.num_face_vertices.size()), offsets(faces.size());
            size_t next = 0;
            for (size_t f = 0; f < faces.size(); ++f) {
                faces[f] = f;
                offsets[f] = next;
                next += shape.mesh.num_face_vertices[f];
            }
            // OBJ often alternates materials face by face. Group within each shape before splitting
            // into bounded draw chunks; this avoids tens of thousands of tiny material switches.
            std::stable_sort(faces.begin(), faces.end(),
                             [&](size_t a, size_t b) { return shape.mesh.material_ids[a] < shape.mesh.material_ids[b]; });
            for (const auto f : faces) {
                const size_t offset = offsets[f];
                const auto count = shape.mesh.num_face_vertices[f];
                if (count != 3)
                    continue;
                const int material = shape.mesh.material_ids[f];
                if (open && (current.material != material || current.count >= 3072))
                    flush();
                if (!open) {
                    current = {uint32_t(result.vertices.size()), 0, material, {}};
                    lo = glm::vec3(std::numeric_limits<float>::max());
                    hi = -lo;
                    open = true;
                }
                std::array<vertex, 3> triangle{};
                for (size_t k = 0; k < 3; ++k) {
                    const auto idx = shape.mesh.indices[offset + k];
                    if (idx.vertex_index < 0 || size_t(idx.vertex_index) * 3 + 2 >= a.vertices.size())
                        throw std::runtime_error("OBJ contains invalid position index");
                    for (int c = 0; c < 3; ++c)
                        triangle[k].position[c] = a.vertices[size_t(idx.vertex_index) * 3 + c] * scale;
                    if (idx.normal_index >= 0)
                        for (int c = 0; c < 3; ++c)
                            triangle[k].normal[c] = a.normals[size_t(idx.normal_index) * 3 + c];
                    if (idx.texcoord_index >= 0)
                        triangle[k].uv = {a.texcoords[size_t(idx.texcoord_index) * 2], 1 - a.texcoords[size_t(idx.texcoord_index) * 2 + 1]};
                }
                const auto cross = glm::cross(triangle[1].position - triangle[0].position, triangle[2].position - triangle[0].position);
                for (auto &v : triangle) {
                    if (glm::dot(v.normal, v.normal) < 1e-12f)
                        v.normal = glm::dot(cross, cross) > 1e-12f ? glm::normalize(cross) : glm::vec3{0, 1, 0};
                    result.vertices.push_back(v);
                    lo = glm::min(lo, v.position);
                    hi = glm::max(hi, v.position);
                    result.minimum = glm::min(result.minimum, v.position);
                    result.maximum = glm::max(result.maximum, v.position);
                }
                current.count += 3;
            }
            flush();
        }
        if (result.vertices.empty())
            throw std::runtime_error("Bistro OBJ has no triangles");
        std::cout << result.vertices.size() / 3 << " triangles, " << result.batches.size() << " caster batches, " << result.materials.size()
                  << " materials\n"
                  << "Bounds: " << result.minimum.x << ',' << result.minimum.y << ',' << result.minimum.z << " to " << result.maximum.x
                  << ',' << result.maximum.y << ',' << result.maximum.z << std::endl;
        return result;
    }
    vkz::texture load_diffuse(vkz::vma_memory_allocator &allocator, VkQueue queue, uint32_t family, const std::filesystem::path &path) {
        auto base = vkz::load(allocator, queue, family, path, VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLER_ADDRESS_MODE_REPEAT);
        vkz::texture result;
        const auto extent = base.image.create_info.extent;
        const uint32_t levels = 1 + uint32_t(std::floor(std::log2(std::max(extent.width, extent.height))));
        result.image = vkz::image::builder(allocator)
            .format(VK_FORMAT_R8G8B8A8_SRGB)
            .extent(extent)
            .mip_levels(levels)
            .usage(vkz::image_usage_sampled_transfer)
            .build();
        vkz::command_pool commands{allocator.device, family, vkz::command_pool_flags_transient, queue};
        auto cmd = commands.create_command_buffer();
        vkz::barrier::push(base.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_all_commands, vkz::pipeline_stage_transfer,
                           vkz::access_memory_read_write, vkz::access_transfer_read, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vkz::barrier::push_and_flush(cmd, result.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, levels, 0, 1}, vkz::pipeline_stage_none, vkz::pipeline_stage_transfer,
                                     vkz::access_none, vkz::access_transfer_write, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageCopy region{};
        region.srcSubresource = {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.extent = extent;
        vkCmdCopyImage(cmd, base.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, result.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                       &region);
        vkz::generate_mip_maps(cmd, result.image);
        vkz::barrier::push_and_flush(cmd, result.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, levels, 0, 1}, vkz::pipeline_stage_all_commands,
                                     vkz::pipeline_stage_fragment_shader, vkz::access_memory_read_write, vkz::access_shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        commands.submit_and_wait(cmd);
        base.destroy();
        result.image_view =
            vkz::image_view::builder(allocator.device)
                .image(result.image)
                .format(VK_FORMAT_R8G8B8A8_SRGB)
                .aspect_mask(vkz::image_aspect_color)
                .level_count(levels)
                .layer_count(1)
                .build();
        result.sampler = vkz::sampler::builder(allocator.device)
                             .mag_filter(VK_FILTER_LINEAR)
                             .min_filter(VK_FILTER_LINEAR)
                             .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                             .max_lod(float(levels - 1))
                             .address_mode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                             .build();
        return result;
    }
    vkz::pipeline make_pipeline(vkz::device device, VkFormat color, VkFormat depth, vkz::descriptor_set_layout shadows,
                                vkz::descriptor_set_layout materials, const char *vert, const char *frag, bool shadow = false,
                                bool sky = false, bool csm_shadow = false) {
        auto path = [](const char *name) { return std::string{VKZ_SVSM_SHADER_DIR} + "/" + name + ".spv"; };
        vkz::graphics_pipeline_builder builder{device};
        builder.shader_stage().vertex_shader(path(vert)).fragment_shader(path(frag));
        if (!sky) {
            builder.vertex_input_state()
                .add_vertex_binding_description(0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX)
                .add_vertex_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, position))
                .add_vertex_attribute_description(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex, uv));
            if (!shadow)
                builder.vertex_input_state().add_vertex_attribute_description(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, normal));
        } else
            builder.vertex_input_state().clear();
        builder.input_assembly_state()
            .triangles()
            .viewport_state()
            .viewport()
            .origin(0, 0)
            .dimension(csm_shadow ? 4096
                       : shadow   ? 8192
                                  : width,
                       csm_shadow ? 4096
                       : shadow   ? 8192
                                  : height)
            .min_depth(0)
            .max_depth(1)
            .scissor()
            .offset(0, 0)
            .extent(csm_shadow ? 4096
                    : shadow   ? 8192
                               : width,
                    csm_shadow ? 4096
                    : shadow   ? 8192
                               : height)
            .add()
            .rasterization_state()
            .cull_none()
            .front_face_counter_clockwise()
            .polygon_mode_fill()
            .multisample_state()
            .rasterization_samples(vkz::sample_count_x1);
        if ((!shadow || csm_shadow) && depth != VK_FORMAT_UNDEFINED) {
            builder.depth_stencil_state().enable_depth_test().compare_op_less_or_equal();
            if (color == VK_FORMAT_UNDEFINED)
                builder.depth_stencil_state().enable_depth_write();
        }
        if (color != VK_FORMAT_UNDEFINED)
            builder.color_blend_state().attachment().add();
        if (csm_shadow) {
            builder.rasterization_state().enable_depth_bias();
            builder.dynamic_state().depth_bias();
        }
        builder.layout().add_descriptor_set_layout(shadows).add_descriptor_set_layout(materials).add_push_constant_range(vkz::shader_stage_vertex | vkz::shader_stage_fragment, 0,
                                                                                                                         sizeof(scene_constants));
        auto &rendering = builder.dynamic_render_pass();
        if (color != VK_FORMAT_UNDEFINED)
            rendering.add_color_attachment(color);
        if (!shadow || csm_shadow)
            rendering.depth_attachment(depth);
        if (csm_shadow)
            rendering.view_mask(15);
        return rendering.build();
    }

    /** @brief Owns the frame lifecycle; callbacks borrow the comparison scene for main_loop(). */
    class bistro_application final : public vkz::application {
      public:
        explicit bistro_application(bool validation)
            : application(
                  vkz::vulkan_app_create_info{.width = width,
                                              .height = height,
                                              .title = "Bistro | SVSM / CSM comparison",
                                              .depth_testing = false,
                                              .multiview = true,
                                              .resizable = false,
                                              .validation = validation,
                                              .vsync = false,
                                              .enabled_features = {.sampler_anisotropy = true, .fragment_stores_and_atomics = true}},
                  1) {}
        vkz::vulkan_app &window_app() { return app_; }
        vkz::vma_memory_allocator allocator() { return allocator_; }
        std::function<void()> next_frame, submitted;
        std::function<void(VkCommandBuffer)> record_scene, present_scene;
        void new_frame() override { next_frame(); }
        void app_logic(VkCommandBuffer cmd) override { record_scene(cmd); }
        void render_swapchain(VkCommandBuffer cmd) override { present_scene(cmd); }
        void end_frame() override { submitted(); }
        VkFormat output_format() const { return color_format(); }
    };
} // namespace

int main(int argc, char **argv) try {
    std::filesystem::path obj = std::filesystem::path{VKZ_SVSM_RESOURCE_DIR} / "bistro/Exterior/exterior.obj";
    std::filesystem::path sky_path = std::filesystem::path{VKZ_SVSM_RESOURCE_DIR} / "svsm_skybox";
    bool validate = false;
    bool show_pages = false;
    int page_view = 0;
    int shadow_method = 0;
    int scene_selection = 0;
    int debug = 0;
    glm::vec3 initial_position{0, 2, 8}, initial_target{0, 2, -8}, initial_sun{};
    bool override_sun = false;
    bool cycle_methods = false;
    bool cycle_scenes = false;
    uint32_t frame_limit = 0;
    float scale = 0.01f;
    std::string screenshot;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--obj" && i + 1 < argc)
            obj = argv[++i];
        else if (arg == "--skybox" && i + 1 < argc)
            sky_path = argv[++i];
        else if (arg == "--frames" && i + 1 < argc)
            frame_limit = uint32_t(std::stoul(argv[++i]));
        else if (arg == "--scale" && i + 1 < argc)
            scale = std::stof(argv[++i]);
        else if ((arg == "--camera" || arg == "--look-at" || arg == "--light-direction") && i + 3 < argc) {
            glm::vec3 value;
            for (int axis = 0; axis < 3; ++axis) {
                value[axis] = std::stof(argv[++i]);
                if (!std::isfinite(value[axis]))
                    throw std::invalid_argument("Camera and light values must be finite");
            }
            if (arg == "--camera")
                initial_position = value;
            else if (arg == "--look-at")
                initial_target = value;
            else {
                if (glm::dot(value, value) < 0.001f)
                    throw std::invalid_argument("Light direction must be nonzero");
                initial_sun = value;
                override_sun = true;
            }
        } else if (arg == "--scene" && i + 1 < argc) {
            const std::string name = argv[++i];
            if (name == "bistro")
                scene_selection = 0;
            else if (name == "floor")
                scene_selection = 1;
            else if (name == "bistro-low-sun")
                scene_selection = 2;
            else
                throw std::invalid_argument("--scene expects bistro, floor, or bistro-low-sun");
        } else if (arg == "--validation")
            validate = true;
        else if (arg == "--shadow-method" && i + 1 < argc) {
            const std::string method = argv[++i];
            if (method == "svsm")
                shadow_method = 0;
            else if (method == "csm")
                shadow_method = 1;
            else if (method == "both")
                shadow_method = 2;
            else if (method == "both-csm")
                shadow_method = 3;
            else
                throw std::invalid_argument("--shadow-method expects svsm, csm, both, or both-csm");
        } else if (arg == "--view" && i + 1 < argc) {
            const std::string view = argv[++i];
            if (view == "lit")
                debug = 0;
            else if (view == "visibility")
                debug = 1;
            else if (view == "coverage")
                debug = 2;
            else
                throw std::invalid_argument("--view expects lit, visibility, or coverage");
        } else if (arg == "--cycle-methods")
            cycle_methods = true;
        else if (arg == "--cycle-scenes")
            cycle_scenes = true;
        else if (arg == "--physical-pages")
            show_pages = true;
        else if (arg == "--physical-depth") {
            show_pages = true;
            page_view = 1;
        } else if (arg == "--screenshot" && i + 1 < argc)
            screenshot = argv[++i];
    }
    if (glm::length(glm::cross(initial_target - initial_position, glm::vec3{0, 1, 0})) < 0.0001f)
        throw std::invalid_argument("Camera target must differ from its position and not be directly above or below it");
    const bool bistro_loaded = scene_selection != 1;
    if (bistro_loaded && !std::filesystem::exists(obj))
        throw std::runtime_error("Bistro missing: copy it to test/resources/bistro or use --obj <path>");
    auto model = bistro_loaded ? load_model(obj, scale) : ::model{};
    if (!bistro_loaded) {
        model.minimum = {-1, 0, -1};
        model.maximum = {1, 0, 1};
    }
    svsm_test::install_logger();
    bistro_application host{validate};
    auto &app = host.window_app();
    auto &context = app.context();
    const auto device = context.device;
    auto allocator = host.allocator();
    const auto queue = app.graphics_queue();
    const auto family = app.queue_family_index();
    vkz::command_pool uploads{device, family, vkz::command_pool_flags_transient, queue};
    auto upload = [&](const void *data, size_t bytes, vkz::buffer_usage usage) {
        const size_t input_bytes = bytes;
        bytes = std::max(bytes, size_t(16));
        auto staging = vkz::buffer::builder(allocator)
            .size(bytes)
            .usage(vkz::buffer_usage_transfer_src)
            .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
            .build();
        auto map = staging.map();
        std::memset(map.as<void>(), 0, bytes);
        if (input_bytes)
            std::memcpy(map.as<void>(), data, input_bytes);
        vmaFlushAllocation(staging.allocator, staging.allocation, 0, bytes);
        map.unmap();
        auto result = vkz::buffer::builder(allocator)
            .size(bytes)
            .usage(usage | vkz::buffer_usage_transfer_dst)
            .build();
        auto cmd = uploads.create_command_buffer();
        VkBufferCopy copy{0, 0, bytes};
        vkCmdCopyBuffer(cmd, staging, result, 1, &copy);
        vkz::barrier::push_and_flush(cmd, vkz::pipeline_stage_transfer, vkz::pipeline_stage_all_commands, vkz::access_transfer_write, vkz::access_memory_read);
        uploads.submit_and_wait(cmd);
        staging.destroy();
        return result;
    };
    auto vertices = upload(model.vertices.data(), model.vertices.size() * sizeof(vertex), vkz::buffer_usage_vertex);
    std::vector<uint32_t> indices(model.vertices.size());
    for (uint32_t i = 0; i < indices.size(); ++i)
        indices[i] = i;
    auto index_buffer = upload(indices.data(), indices.size() * 4, vkz::buffer_usage_index);
    indices.clear();
    indices.shrink_to_fit();
    model.vertices.clear();
    model.vertices.shrink_to_fit();
    std::vector<vkz::svsm::caster_bounds> bounds;
    std::vector<VkDrawIndexedIndirectCommand> templates;
    for (auto &b : model.batches) {
        bounds.push_back(b.bounds);
        templates.push_back({b.count, 1, b.first, 0, 0});
    }
    auto caster_buffer = upload(bounds.data(), bounds.size() * sizeof(bounds[0]), vkz::buffer_usage_storage);
    auto draw_buffer = upload(templates.data(), templates.size() * sizeof(templates[0]), vkz::buffer_usage_storage);
    const auto output_format = host.output_format();
    vkz::texture output;
    output.image = vkz::image::builder(allocator)
        .format(output_format)
        .extent(width, height)
        .usage(vkz::image_usage_color | vkz::image_usage_sampled | vkz::image_usage_transfer_src)
        .build();
    output.image_view = vkz::image_view::builder(device)
        .image(output.image)
        .format(output_format)
        .aspect_mask(vkz::image_aspect_color)
        .level_count(1)
        .layer_count(1)
        .build();
    output.sampler = vkz::sampler::builder(device).build();
    auto output_layout = vkz::make_descriptor_set_layout_builder(device)
                             .binding(0)
                             .descriptor_count(1)
                             .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                             .shader_stages(vkz::shader_stage_fragment)
                             .create_layout();
    vkz::descriptor_pool output_pool{device, 1, {{static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 1}}};
    auto output_set = output_pool.allocate(output_layout);
    vkz::update_descriptors(
        device, {{output_set, {vkz::texture_descriptor{output.image_view, output.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0}}}});
    vkz::buffer screenshot_buffer;
    if (!screenshot.empty()) screenshot_buffer = vkz::buffer::builder(allocator)
        .size(width * height * 4)
        .usage(vkz::buffer_usage_transfer_dst)
        .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
        .build();
    vkz::texture depth;
    const auto depth_format = vkz::pick_depth_format(device.physical);
    depth.image = vkz::image::builder(allocator)
        .format(depth_format)
        .extent(width, height)
        .usage(vkz::image_usage_depth_stencil_sampled)
        .build();
    depth.image_view = vkz::image_view::builder(device)
        .image(depth.image)
        .format(depth_format)
        .aspect_mask(vkz::image_aspect_depth)
        .level_count(1)
        .layer_count(1)
        .build();
    const auto shadows = vkz::svsm::create({.device = device,
                                            .memory_allocator = allocator,
                                            .physical_pages = 4096,
                                            .max_casters = std::max(1u, uint32_t(bounds.size())),
                                            .in_flight_frames = 1,
                                            .first_clipmap_extent = 8,
                                            .fragment_stores_and_atomics_enabled = true});
    vkz::texture sky, irradiance;
    if (bistro_loaded) {
        sky = vkz::texture_from_ktx(sky_path / "skybox_diffuse.ktx", allocator, queue, family, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        irradiance =
            vkz::texture_from_ktx(sky_path / "skybox_irradiance.ktx", allocator, queue, family, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    }
    std::vector<vkz::texture> textures(1);
    auto &white = textures.front();
    white.image = vkz::image::builder(allocator)
        .format(VK_FORMAT_R8G8B8A8_SRGB)
        .extent(1, 1)
        .usage(vkz::image_usage_sampled_upload)
        .build();
    white.image_view = vkz::image_view::builder(device)
        .image(white.image)
        .format(VK_FORMAT_R8G8B8A8_SRGB)
        .aspect_mask(vkz::image_aspect_color)
        .level_count(1)
        .layer_count(1)
        .build();
    white.sampler = vkz::sampler::builder(device).build();
    {
        auto cmd = uploads.create_command_buffer();
        vkz::clear(cmd, white.image, VkClearColorValue{{1, 1, 1, 1}}, {});
        vkz::barrier::push_and_flush(cmd, white.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_transfer, vkz::pipeline_stage_fragment_shader,
                                     vkz::access_transfer_write, vkz::access_shader_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        uploads.submit_and_wait(cmd);
    }
    std::vector<uint32_t> material_textures(model.materials.size() + 1, 0);
    std::unordered_map<std::string, uint32_t> loaded;
    for (uint32_t i = 0; i < model.materials.size(); ++i) {
        auto name = model.materials[i].diffuse_texname;
        if (name.empty())
            continue;
        std::replace(name.begin(), name.end(), '\\', '/');
        auto path = (obj.parent_path() / name).lexically_normal();
        if (const auto found = loaded.find(path.string()); found != loaded.end()) {
            material_textures[i] = found->second;
            continue;
        }
        if (!std::filesystem::exists(path)) {
            std::cerr << "Missing texture: " << path << '\n';
            continue;
        }
        material_textures[i] = uint32_t(textures.size());
        loaded[path.string()] = material_textures[i];
        textures.push_back(load_diffuse(allocator, queue, family, path));
    }
    std::cout << (bistro_loaded ? "Bistro assets uploaded" : "Infinite floor: no external assets or shadow casters") << std::endl;
    auto material_layout = vkz::make_descriptor_set_layout_builder(device)
                               .binding(0)
                               .descriptor_count(1)
                               .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                               .shader_stages(vkz::shader_stage_fragment)
                               .binding(1)
                               .descriptor_count(1)
                               .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                               .shader_stages(vkz::shader_stage_fragment)
                               .binding(2)
                               .descriptor_count(1)
                               .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                               .shader_stages(vkz::shader_stage_fragment)
                               .create_layout();
    vkz::descriptor_pool material_pool{
        device, uint32_t(material_textures.size()), {{static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), uint32_t(material_textures.size() * 3)}}};
    auto materials = material_pool.allocate_n(material_layout, material_textures.size());
    for (uint32_t i = 0; i < materials.size(); ++i) {
        if (!bistro_loaded) {
            vkz::update_descriptors(device, {{materials[i], {vkz::descriptor<vkz::texture_descriptor>(textures.front(), 0)}}});
            continue;
        }
        vkz::update_descriptors(device, {{materials[i],
                                          {vkz::descriptor<vkz::texture_descriptor>(textures[material_textures[i]], 0), vkz::descriptor<vkz::texture_descriptor>(sky, 1),
                                           vkz::descriptor<vkz::texture_descriptor>(irradiance, 2)}}});
    }
    vkz::descriptor_set_layout shadow_layout{vkz::svsm::descriptor_set_layout(shadows), device};
    vkz::descriptor_pool csm_pool{device,
                                  2,
                                  {{static_cast<VkDescriptorType>(vkz::descriptor_type_storage_buffer), 4},
                                   {static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 2},
                                   {static_cast<VkDescriptorType>(vkz::descriptor_type_uniform_buffer), 1}}};
    auto csm_initial_transition = uploads.create_command_buffer();
    const auto cascades =
        vkz::csm::create({.device = device,
                          .depth_format = depth_format,
                          .memory_allocator = allocator,
                          .vertex_shader_include = "layout(location=0) in vec3 position; mat4 get_model_matrix(){ return mat4(1); }",
                          .vertex_shader_position_stride = sizeof(vertex),
                          .vertex_include_descriptorset_layout = material_layout.handle,
                          .descriptor_pool = csm_pool.handle(),
                          .initial_transition_command_buffer = csm_initial_transition,
                          .in_flight_frames = 1,
                          .num_cascades = 4,
                          .size = 4096,
                          .cull_mode = vkz::cull_mode_none});
    uploads.submit_and_wait(csm_initial_transition);
    auto csm_camera_buffer = vkz::buffer::builder(allocator)
        .size(80)
        .usage(vkz::buffer_usage_uniform)
        .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
        .build();
    auto csm_lighting_layout = vkz::make_descriptor_set_layout_builder(device)
                                   .binding(0)
                                   .descriptor_count(1)
                                   .descriptor_type(vkz::descriptor_type_storage_buffer)
                                   .shader_stages(vkz::shader_stage_fragment)
                                   .binding(1)
                                   .descriptor_count(1)
                                   .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                                   .shader_stages(vkz::shader_stage_fragment)
                                   .binding(2)
                                   .descriptor_count(1)
                                   .descriptor_type(vkz::descriptor_type_uniform_buffer)
                                   .shader_stages(vkz::shader_stage_fragment)
                                   .create_layout();
    auto csm_lighting_set = csm_pool.allocate(csm_lighting_layout);
    const auto csm_texture = vkz::csm::shadow_map(cascades, 0);
    vkz::update_descriptors(device, {{csm_lighting_set,
                                      {vkz::descriptor<vkz::buffer_descriptor>(vkz::csm::cascade_view_projection(cascades), 0),
                                       vkz::texture_descriptor{csm_texture.image_view, csm_texture.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1},
                                       vkz::descriptor<vkz::ubo_descriptor>(csm_camera_buffer, 2)}}});
    auto csm_forward =
        make_pipeline(device, output_format, depth_format, csm_lighting_layout, material_layout, "bistro.vert", "bistro_csm.frag");
    auto csm_shadow = make_pipeline(device, VK_FORMAT_UNDEFINED, depth_format, {vkz::csm::descriptor_set_layout(cascades), device},
                                    material_layout, "bistro_csm_shadow.vert", "bistro_depth.frag", true, false, true);
    auto prepass =
        make_pipeline(device, VK_FORMAT_UNDEFINED, depth_format, shadow_layout, material_layout, "bistro.vert", "bistro_depth.frag");
    auto forward = make_pipeline(device, output_format, depth_format, shadow_layout, material_layout, "bistro.vert", "bistro.frag");
    auto shadow = make_pipeline(device, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, shadow_layout, material_layout, "bistro_shadow.vert",
                                "bistro_shadow.frag", true);
    auto sky_pipeline = make_pipeline(device, output_format, depth_format, shadow_layout, material_layout, "bistro_sky.vert",
                                      "bistro_sky.frag", false, true);
    auto pages_pipeline = make_pipeline(device, output_format, VK_FORMAT_UNDEFINED, shadow_layout, material_layout, "bistro_sky.vert",
                                        "bistro_pages.frag", false, true);
    auto floor_prepass = make_pipeline(device, VK_FORMAT_UNDEFINED, depth_format, shadow_layout, material_layout, "floor.vert",
                                       "floor_depth.frag", false, true);
    auto floor_svsm =
        make_pipeline(device, output_format, depth_format, shadow_layout, material_layout, "floor.vert", "floor_svsm.frag", false, true);
    auto floor_csm = make_pipeline(device, output_format, depth_format, csm_lighting_layout, material_layout, "floor.vert",
                                   "floor_csm.frag", false, true);
    vkz::glfw_input_adaptor input(app.window(), false);
    input.bind();
    vkz::camera::camera camera;
    camera.position = initial_position;
    camera.velocity = {6, 6, 6};
    camera.acceleration = {12, 12, 12};
    camera.rotationSpeed = 0.15f;
    vkz::camera::spectator initializer(camera);
    initializer.look_at(camera.position, initial_target, {0, 1, 0});
    initializer.perspective(65, float(width) / height, 0.05f, 2000);
    vkz::camera::controller controller{camera, vkz::camera::movement_type::spectator, input.get_device()};
    auto stats_readback = vkz::buffer::builder(allocator)
        .size(sizeof(vkz::svsm::statistics))
        .usage(vkz::buffer_usage_transfer_dst)
        .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
        .build();
    vkz::profiler gpu_profiler{device, 16};
    std::vector<std::string> profile_names{"Receiver depth", "SVSM total", "CSM total", "SVSM lighting", "CSM lighting"};
    std::vector<std::string> recorded_queries;
    bool recorded_svsm = false;
    for (auto name : vkz::svsm::profiling_queries)
        profile_names.emplace_back(name);
    for (const auto &name : profile_names)
        gpu_profiler.add_query(name);
    auto collect_profile = [&] {
        // This viewer has one in-flight slot. Its fence must complete before reading or reusing queries.
        gpu_profiler.commit(recorded_queries);
        for (auto &[name, query] : gpu_profiler.queries)
            if (query.runtimes.size() > 240)
                query.runtimes.erase(query.runtimes.begin());
    };
    const test_timeout timeout{argc, argv}; // Timeout starts AFTER loading large assets.
    uint32_t frame = 0;
    float page_size_on_screen = float(height) - 48.0f;
    bool force_uncached = false;
    float csm_bias_constant = 1.25f, csm_bias_slope = 2.0f;
    auto preset_sun = [](int scene) {
        /** Light travels downwards: a negative Y component places the sun above the horizon. */
        return scene == 2 ? glm::normalize(glm::vec3{0.8f, -std::tan(glm::radians(5.0f)), 0.6f})
                          : glm::normalize(glm::vec3{0.4f, -0.8f, 0.3f});
    };
    glm::vec3 sun = override_sun ? initial_sun : preset_sun(scene_selection);
    vkz::svsm::statistics stats{};
    auto previous = std::chrono::steady_clock::now();
    host.next_frame = [&] {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;
        controller.process_input();
        controller.update(std::min(dt, 0.1f));
        if (frame) {
            collect_profile();
            if (recorded_svsm) {
                vmaInvalidateAllocation(stats_readback.allocator, stats_readback.allocation, 0, VK_WHOLE_SIZE);
                auto m = stats_readback.map();
                stats = *m.as<vkz::svsm::statistics>();
                m.unmap();
            }
        }
        if (cycle_methods)
            shadow_method = int(frame % 4);
        ImGui::SetNextWindowSizeConstraints({390, show_pages ? 420.0f : 220.0f}, {float(width), float(height)});
        ImGui::Begin("Shadow comparison");
        bool scene_changed = false;
        if (cycle_scenes && bistro_loaded) {
            const int next_scene = int((frame / 4) % 3);
            scene_changed = next_scene != scene_selection;
            scene_selection = next_scene;
        }
        const char *scene_names[]{"Bistro", "Infinite floor", "Bistro - low sun (5 degrees)"};
        if (ImGui::BeginCombo("Scene", scene_names[scene_selection])) {
            ImGui::BeginDisabled(!bistro_loaded);
            if (ImGui::Selectable("Bistro", scene_selection == 0) && scene_selection != 0) {
                scene_selection = 0;
                scene_changed = true;
            }
            if (ImGui::Selectable(scene_names[2], scene_selection == 2) && scene_selection != 2) {
                scene_selection = 2;
                scene_changed = true;
            }
            ImGui::EndDisabled();
            if (ImGui::Selectable("Infinite floor", scene_selection == 1) && scene_selection != 1) {
                scene_selection = 1;
                scene_changed = true;
            }
            ImGui::EndCombo();
        }
        if (scene_changed) {
            sun = preset_sun(scene_selection);
            vkz::svsm::reset(shadows);
            gpu_profiler.clear_runtimes();
            stats = {};
        }
        if (!bistro_loaded)
            ImGui::TextUnformatted("Launch with --scene bistro to also load Bistro.");
        ImGui::Combo("Method", &shadow_method, "SVSM only\0CSM only\0Profile both / show SVSM\0Profile both / show CSM\0");
        ImGui::TextUnformatted("CSM: 4 x 4096 | SVSM: 6 x 8192 virtual, 4096 pages");
        ImGui::TextUnformatted("Both: same camera and alpha test; 3x3 PCF.");
        ImGui::TextUnformatted("Different coverage/resolution; compare quality as well as time.");
        if (shadow_method != 0) {
            ImGui::SliderFloat("CSM constant bias", &csm_bias_constant, 0.0f, 4.0f, "%.2f");
            ImGui::SliderFloat("CSM slope bias", &csm_bias_slope, 0.0f, 4.0f, "%.2f");
        }
        ImGui::TextUnformatted("WASD, Q/E, left-drag to look");
        ImGui::Text("%.1f ms | %zu caster batches", dt * 1000, scene_selection != 1 ? model.batches.size() : size_t(0));
        ImGui::Text("Pages: %u requested / %u rendered / %u cached / %u missing", stats.requested, stats.rendered, stats.cached,
                    stats.missing);
        ImGui::Text("Pool: %u resident / %u evicted | %u caster-level draws", stats.resident, stats.evicted, stats.visible_casters);
        ImGui::Combo("View", &debug, "Lit\0Shadow visibility\0Coverage\0");
        ImGui::Checkbox("Physical page atlas", &show_pages);
        if (show_pages) {
            ImGui::Combo("Page contents", &page_view, "Residency\0Stored depth\0");
            ImGui::SliderFloat("Atlas size", &page_size_on_screen, 256.0f, float(height) - 48.0f, "%.0f px");
            ImGui::TextColored({1, 0.48f, 0.05f, 1}, "Orange: rendered this frame (allocated or refreshed)");
            ImGui::TextColored({0.12f, 0.8f, 0.3f, 1}, "Green: requested and cached");
            ImGui::TextColored({0.15f, 0.35f, 0.8f, 1}, "Blue: retained, not requested");
            ImGui::Text("Dark: free | Red: invalid | Purple: unused capacity");
            ImGui::Text("Physical slots run left to right, then top to bottom.");
            ImGui::Text("Move the camera to see allocation, reuse, and release.");
        }
        ImGui::Checkbox("Invalidate every frame", &force_uncached);
        if (ImGui::SliderFloat3("Light direction", &sun.x, -1, 1) && glm::dot(sun, sun) < 0.001f)
            sun = {0, -1, 0};
        if (ImGui::Button("Reset cache"))
            vkz::svsm::reset(shadows);
        ImGui::Text("Camera %.2f %.2f %.2f", camera.position.x, camera.position.y, camera.position.z);
        ImGui::Text("View direction %.3f %.3f %.3f", camera.viewDir.x, camera.viewDir.y, camera.viewDir.z);
        ImGui::End();
        ImGui::SetNextWindowPos({float(width) - 650, 24}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({620, 360}, ImGuiCond_FirstUseEver);
        ImGui::Begin("GPU shadow profiling");
        ImGui::TextUnformatted("VSync off | application-managed frames");
        ImGui::Text("Last 240 samples per row | milliseconds | SVSM %zu / CSM %zu", gpu_profiler.queries.at("SVSM total").runtimes.size(),
                    gpu_profiler.queries.at("CSM total").runtimes.size());
        if (validate)
            ImGui::TextUnformatted("Validation enabled: timings include validation overhead.");
        if (ImGui::Button("Reset timing history"))
            gpu_profiler.clear_runtimes();
        if (ImGui::BeginTable("GPU timings", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthFixed, 170.0f);
            for (auto label : {"Average", "Min", "Max", "Std dev"})
                ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            auto timings = gpu_profiler.query_stats();
            for (const auto &name : profile_names) {
                const auto &timing = timings.at(name);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name.c_str());
                for (float value : {timing.mean, timing.minimum, timing.maximum, timing.standard_deviation}) {
                    ImGui::TableNextColumn();
                    if (gpu_profiler.queries.at(name).runtimes.empty())
                        ImGui::TextUnformatted("--");
                    else
                        ImGui::Text("%.3f", value);
                }
            }
            ImGui::EndTable();
        }
        ImGui::End();
    };
    host.record_scene = [&](VkCommandBuffer cmd) {
        const bool floor_scene = scene_selection == 1;
        const bool run_svsm = shadow_method != 1, run_csm = shadow_method != 0;
        const bool show_csm = shadow_method == 1 || shadow_method == 3;
        recorded_svsm = run_svsm;
        recorded_queries = {"Receiver depth", show_csm ? "CSM lighting" : "SVSM lighting"};
        if (run_svsm) {
            recorded_queries.emplace_back("SVSM total");
            for (auto name : vkz::svsm::profiling_queries)
                recorded_queries.emplace_back(name);
        }
        if (run_csm)
            recorded_queries.emplace_back("CSM total");
        vkz::barrier::push_and_flush(cmd, depth.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_depth), 0, 1, 0, 1}, vkz::pipeline_stage_all_commands,
                                     vkz::pipeline_stage_early_fragment_tests | vkz::pipeline_stage_late_fragment_tests, vkz::access_memory_read_write,
                                     vkz::access_depth_stencil_attachment_write, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        scene_constants constants{};
        constants.matrix = camera.projection * camera.view;
        constants.light_direction = glm::vec4(glm::normalize(sun), 0);
        constants.info.z = uint32_t(debug);
        auto draw_scene = [&](const vkz::pipeline &pipeline, bool indirect, uint32_t clip) {
            if (floor_scene)
                return;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
            if (pipeline.handle == csm_shadow.handle)
                vkCmdSetDepthBias(cmd, csm_bias_constant, 0.0f, csm_bias_slope);
            VkDeviceSize zero = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertices._, &zero);
            vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT32);
            for (uint32_t b = 0; b < model.batches.size(); ++b) {
                const auto &batch = model.batches[b];
                uint32_t m = batch.material >= 0 ? uint32_t(batch.material) : uint32_t(model.materials.size());
                constants.diffuse = glm::vec4{1};
                if (m < model.materials.size()) {
                    auto &mat = model.materials[m];
                    constants.diffuse = {mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], mat.dissolve};
                }
                constants.info.x = clip;
                constants.info.y = b;
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 1, 1, &materials[m].handle, 0, nullptr);
                vkCmdPushConstants(cmd, pipeline.layout, static_cast<VkShaderStageFlags>(vkz::shader_stage_vertex | vkz::shader_stage_fragment), 0, sizeof(constants), &constants);
                if (indirect)
                    vkCmdDrawIndexedIndirect(cmd, vkz::svsm::indexed_draws(shadows, 0),
                                             (VkDeviceSize(clip) * model.batches.size() + b) * 20, 1, 20);
                else
                    vkCmdDrawIndexed(cmd, batch.count, 1, batch.first, 0, 0);
            }
        };
        vkz::render_info render{};
        render.render_area = {width, height};
        render.depth_attachment = vkz::depth_stencil_attachment{depth.image_view, depth_format, {1, 0}, true};
        auto draw_floor = [&](const vkz::pipeline &pipeline) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
            vkCmdPushConstants(cmd, pipeline.layout, static_cast<VkShaderStageFlags>(vkz::shader_stage_vertex | vkz::shader_stage_fragment), 0, sizeof(constants), &constants);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        };
        gpu_profiler.profile(
            "Receiver depth", cmd,
            [&] {
                vkz::render(cmd, render, [&] {
                    if (floor_scene)
                        draw_floor(floor_prepass);
                    else
                        draw_scene(prepass, false, 0);
                });
            },
            vkz::pipeline_stage_bottom_of_pipe);
        vkz::barrier::push_and_flush(cmd, depth.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_depth), 0, 1, 0, 1},
                                     vkz::pipeline_stage_early_fragment_tests | vkz::pipeline_stage_late_fragment_tests, vkz::pipeline_stage_compute_shader,
                                     vkz::access_depth_stencil_attachment_write, vkz::access_shader_read, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        const float depth_extent = floor_scene ? 1000000.0f : glm::length(glm::max(glm::abs(model.minimum), glm::abs(model.maximum))) + 10;
        auto shadow_set = vkz::svsm::descriptor_set(shadows, 0);
        if (run_svsm)
            gpu_profiler.profile(
                "SVSM total", cmd,
                [&] {
                    vkz::svsm::begin(shadows, cmd,
                                     {.depth = depth,
                                      .inverse_view_projection = glm::inverse(constants.matrix),
                                      .camera_position = camera.position,
                                      .light_direction = sun,
                                      .light_depth_min = -depth_extent,
                                      .light_depth_max = depth_extent,
                                      .casters = caster_buffer,
                                      .caster_count = floor_scene ? 0u : uint32_t(bounds.size()),
                                      .indexed_draws = draw_buffer,
                                      .reset_cache = force_uncached,
                                      .gpu_profiler = &gpu_profiler});
                    vkz::svsm::capture(shadows, cmd, [&](const vkz::svsm::render_context &pass) {
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow.layout, 0, 1, &shadow_set, 0, nullptr);
                        draw_scene(shadow, true, pass.clipmap);
                    });
                    vkz::svsm::end(shadows, cmd);
                },
                vkz::pipeline_stage_bottom_of_pipe);
        if (run_csm) {
            struct csm_camera_data {
                glm::mat4 view;
                glm::vec4 splits;
            } data{camera.view, {}};
            vkz::csm::update(cascades, {constants.matrix, 0.05f, 2000.0f}, -glm::normalize(sun), {&data.splits.x, 4});
            auto matrices = vkz::csm::cascade_view_projection(cascades);
            if (!floor_scene) {
                /** Retain off-camera casters toward the sun, using the same scene depth coverage as SVSM. */
                auto mapping = matrices.map();
                auto *transforms = mapping.as<glm::mat4>();
                const glm::vec3 depth_axis = glm::normalize(sun) / (2.0f * depth_extent);
                for (uint32_t level = 0; level < 4; ++level) {
                    for (int axis = 0; axis < 3; ++axis)
                        transforms[level][axis][2] = depth_axis[axis];
                    transforms[level][3][2] = 0.5f;
                }
                mapping.unmap();
            }
            vmaFlushAllocation(matrices.allocator, matrices.allocation, 0, VK_WHOLE_SIZE);
            auto mapping = csm_camera_buffer.map();
            std::memcpy(mapping.as<void>(), &data, sizeof(data));
            vmaFlushAllocation(csm_camera_buffer.allocator, csm_camera_buffer.allocation, 0, VK_WHOLE_SIZE);
            mapping.unmap();
            gpu_profiler.profile(
                "CSM total", cmd,
                [&] {
                    vkz::csm::capture(
                        cascades,
                        [&](VkPipelineLayout) {
                            auto set = vkz::csm::descriptor_set(cascades);
                            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, csm_shadow.layout, 0, 1, &set, 0, nullptr);
                            draw_scene(csm_shadow, false, 0);
                        },
                        cmd, 0);
                },
                vkz::pipeline_stage_bottom_of_pipe);
        }
        vkz::barrier::push_and_flush(cmd, depth.image, {static_cast<VkImageAspectFlags>(vkz::image_aspect_depth), 0, 1, 0, 1}, vkz::pipeline_stage_compute_shader,
                                     vkz::pipeline_stage_early_fragment_tests | vkz::pipeline_stage_late_fragment_tests, vkz::access_shader_read, vkz::access_depth_stencil_attachment_read,
                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        auto &color = output.image;
        vkz::barrier::push_and_flush(cmd, color.handle, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_all_commands,
                                     vkz::pipeline_stage_color_attachment_output, vkz::access_memory_read_write, vkz::access_color_attachment_write, color.layout,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        render.color_attachments.push_back({output.image_view, output_format, {0.10f, 0.14f, 0.20f, 1}});
        render.depth_attachment->clear = false;
        gpu_profiler.profile(
            show_csm ? "CSM lighting" : "SVSM lighting", cmd,
            [&] {
                vkz::render(cmd, render, [&] {
                    if (floor_scene) {
                        auto &floor_pipeline = show_csm ? floor_csm : floor_svsm;
                        const auto set = show_csm ? csm_lighting_set.handle : shadow_set;
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, floor_pipeline.layout, 0, 1, &set, 0, nullptr);
                        draw_floor(floor_pipeline);
                        return;
                    }
                    auto &lighting = show_csm ? csm_forward : forward;
                    auto lighting_set = show_csm ? csm_lighting_set.handle : shadow_set;
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lighting.layout, 0, 1, &lighting_set, 0, nullptr);
                    draw_scene(lighting, false, 0);
                    scene_constants sky_constants{};
                    sky_constants.matrix = glm::inverse(camera.projection * camera.view);
                    sky_constants.diffuse = glm::vec4(camera.position, 1);
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sky_pipeline.handle);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sky_pipeline.layout, 1, 1, &materials.back().handle, 0,
                                            nullptr);
                    vkCmdPushConstants(cmd, sky_pipeline.layout, static_cast<VkShaderStageFlags>(vkz::shader_stage_vertex | vkz::shader_stage_fragment), 0, sizeof(sky_constants), &sky_constants);
                    vkCmdDraw(cmd, 3, 1, 0, 0);
                });
            },
            vkz::pipeline_stage_bottom_of_pipe);
        // ImGui's pipeline declares no depth attachment; use a separate color-only rendering scope.
        vkz::barrier::push_and_flush(cmd, vkz::pipeline_stage_color_attachment_output, vkz::pipeline_stage_color_attachment_output, vkz::access_color_attachment_write,
                                     vkz::access_color_attachment_read_write);
        render.depth_attachment.reset();
        render.color_attachments[0].clear = false;
        vkz::render(cmd, render, [&] {
            if (show_pages && run_svsm) {
                scene_constants page_constants{};
                page_constants.diffuse = {float(width) - page_size_on_screen - 24.0f, 24.0f, page_size_on_screen, 0};
                page_constants.info.z = uint32_t(page_view);
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pages_pipeline.handle);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pages_pipeline.layout, 0, 1, &shadow_set, 0, nullptr);
                vkCmdPushConstants(cmd, pages_pipeline.layout, static_cast<VkShaderStageFlags>(vkz::shader_stage_vertex | vkz::shader_stage_fragment), 0, sizeof(page_constants), &page_constants);
                vkCmdDraw(cmd, 3, 1, 0, 0);
            }
            vkz::imgui::render(cmd);
        });
        VkBufferCopy copy{0, 0, sizeof(stats)};
        if (run_svsm)
            vkCmdCopyBuffer(cmd, vkz::svsm::counters(shadows, 0), stats_readback, 1, &copy);
        vkz::barrier::push_and_flush(cmd, vkz::pipeline_stage_transfer, vkz::pipeline_stage_host, vkz::access_transfer_write, vkz::access_host_read);
        if (screenshot_buffer._) {
            vkz::barrier::push_and_flush(cmd, color.handle, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_color_attachment_output,
                                         vkz::pipeline_stage_transfer, vkz::access_color_attachment_write, vkz::access_transfer_read, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkBufferImageCopy image_copy{};
            image_copy.imageSubresource = {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 0, 1};
            image_copy.imageExtent = {width, height, 1};
            vkCmdCopyImageToBuffer(cmd, color.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, screenshot_buffer, 1, &image_copy);
            vkz::barrier::push_and_flush(cmd, vkz::pipeline_stage_transfer, vkz::pipeline_stage_host, vkz::access_transfer_write, vkz::access_host_read);
            vkz::barrier::push_and_flush(cmd, color.handle, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_transfer,
                                         vkz::pipeline_stage_fragment_shader, vkz::access_transfer_read, vkz::access_shader_sampled_read, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        } else
            vkz::barrier::push_and_flush(cmd, color.handle, {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, 0, 1}, vkz::pipeline_stage_color_attachment_output,
                                         vkz::pipeline_stage_fragment_shader, vkz::access_color_attachment_write, vkz::access_shader_sampled_read, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        output.image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    };
    auto output_pipeline = make_pipeline(device, output_format, VK_FORMAT_UNDEFINED, output_layout, material_layout, "bistro_sky.vert",
                                         "bistro_present.frag", false, true);
    host.present_scene = [&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, output_pipeline.handle);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, output_pipeline.layout, 0, 1, &output_set.handle, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    };
    host.submitted = [&] {
        ++frame;
        if (timeout.expired() || (frame_limit && frame >= frame_limit))
            glfwSetWindowShouldClose(app.window(), GLFW_TRUE);
    };
    host.main_loop();
    vkDeviceWaitIdle(device);
    if (frame)
        collect_profile();
    for (const auto &[name, timing] : gpu_profiler.query_stats())
        std::cout << name << " GPU ms: average=" << timing.mean << " min=" << timing.minimum << " max=" << timing.maximum
                  << " stddev=" << timing.standard_deviation << '\n';
    gpu_profiler.deinit();
    std::cout << "Bistro rendered " << frame << " frames; last pages rendered=" << stats.rendered << ", cached=" << stats.cached
              << ", missing=" << stats.missing << '\n';
    if (screenshot_buffer._ && frame) {
        vmaInvalidateAllocation(screenshot_buffer.allocator, screenshot_buffer.allocation, 0, VK_WHOLE_SIZE);
        auto m = screenshot_buffer.map();
        auto *pixels = m.as<uint8_t>();
        if (output_format == VK_FORMAT_B8G8R8A8_SRGB || output_format == VK_FORMAT_B8G8R8A8_UNORM)
            for (uint32_t i = 0; i < width * height; ++i)
                std::swap(pixels[i * 4], pixels[i * 4 + 2]);
        if (!stbi_write_png(screenshot.c_str(), width, height, 4, pixels, width * 4))
            throw std::runtime_error("Could not write screenshot");
        m.unmap();
    }
    screenshot_buffer.destroy();
    output_pipeline.destroy();
    output_pool.destroy();
    output_layout.destroy();
    output.destroy();
    prepass.destroy();
    forward.destroy();
    shadow.destroy();
    sky_pipeline.destroy();
    pages_pipeline.destroy();
    floor_prepass.destroy();
    floor_svsm.destroy();
    floor_csm.destroy();
    csm_forward.destroy();
    csm_shadow.destroy();
    csm_pool.destroy();
    csm_lighting_layout.destroy();
    csm_camera_buffer.destroy();
    vkz::csm::destroy(cascades);
    material_pool.destroy();
    material_layout.destroy();
    for (auto &t : textures)
        t.destroy();
    if (bistro_loaded) {
        sky.destroy();
        irradiance.destroy();
    }
    vkz::svsm::destroy(shadows);
    depth.destroy();
    vertices.destroy();
    index_buffer.destroy();
    caster_buffer.destroy();
    draw_buffer.destroy();
    stats_readback.destroy();
    svsm_test::check_validation();
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
