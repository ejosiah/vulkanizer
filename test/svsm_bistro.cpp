#define VKZ_IOSTREAM_ADAPTER
#include "svsm_validation.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "third_party/tinyobjloader/tiny_obj_loader.h"
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
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/builders.hpp>
#include <vulkanizer/commands.hpp>
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
                           .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                           .build();
        vkz::command_pool commands{allocator.device, family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue};
        auto cmd = commands.create_command_buffer();
        vkz::barrier::push(base.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                           VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                           VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vkz::barrier::push_and_flush(cmd, result.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1}, VK_PIPELINE_STAGE_2_NONE,
                                     VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageCopy region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.extent = extent;
        vkCmdCopyImage(cmd, base.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, result.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                       &region);
        vkz::generate_mip_maps(cmd, result.image);
        vkz::barrier::push_and_flush(cmd, result.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1}, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                                     VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        commands.submit_and_wait(cmd);
        base.destroy();
        result.image_view = vkz::image_view::builder(allocator.device)
                                .image(result.image)
                                .format(VK_FORMAT_R8G8B8A8_SRGB)
                                .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
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
                                bool sky = false) {
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
            .dimension(shadow ? 8192 : width, shadow ? 8192 : height)
            .min_depth(0)
            .max_depth(1)
            .scissor()
            .offset(0, 0)
            .extent(shadow ? 8192 : width, shadow ? 8192 : height)
            .add()
            .rasterization_state()
            .cull_none()
            .front_face_counter_clockwise()
            .polygon_mode_fill()
            .multisample_state()
            .rasterization_samples(VK_SAMPLE_COUNT_1_BIT);
        if (!shadow && depth != VK_FORMAT_UNDEFINED) {
            builder.depth_stencil_state().enable_depth_test().compare_op_less_or_equal();
            if (color == VK_FORMAT_UNDEFINED)
                builder.depth_stencil_state().enable_depth_write();
        }
        if (color != VK_FORMAT_UNDEFINED)
            builder.color_blend_state().attachment().add();
        builder.layout().add_descriptor_set_layout(shadows).add_descriptor_set_layout(materials).add_push_constant_range(
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(scene_constants));
        auto &rendering = builder.dynamic_render_pass();
        if (color != VK_FORMAT_UNDEFINED)
            rendering.add_color_attachment(color);
        if (!shadow)
            rendering.depth_attachment(depth);
        return rendering.build();
    }
} // namespace

int main(int argc, char **argv) try {
    std::filesystem::path obj = std::filesystem::path{VKZ_SVSM_RESOURCE_DIR} / "bistro/Exterior/exterior.obj";
    std::filesystem::path sky_path = std::filesystem::path{VKZ_SVSM_RESOURCE_DIR} / "svsm_skybox";
    bool validate = false;
    bool show_pages = false;
    int page_view = 0;
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
        else if (arg == "--validation")
            validate = true;
        else if (arg == "--physical-pages")
            show_pages = true;
        else if (arg == "--physical-depth") {
            show_pages = true;
            page_view = 1;
        }
        else if (arg == "--screenshot" && i + 1 < argc)
            screenshot = argv[++i];
    }
    if (!std::filesystem::exists(obj))
        throw std::runtime_error("Bistro missing: copy it to test/resources/bistro or use --obj <path>");
    auto model = load_model(obj, scale);
    svsm_test::install_logger();
    vkz::vulkan_app app{{.width = width,
                         .height = height,
                         .title = "Bistro | Sparse virtual shadow maps",
                         .resizable = false,
                         .validation = validate,
                         .enabled_features = {.fragment_stores_and_atomics = true}}};
    auto &context = app.context();
    const auto device = context.device;
    auto allocator = vkz::vma_memory_allocator::create(context);
    const auto queue = app.graphics_queue();
    const auto family = app.queue_family_index();
    vkz::command_pool uploads{device, family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue};
    auto upload = [&](const void *data, size_t bytes, VkBufferUsageFlags usage) {
        auto staging = vkz::buffer::builder(allocator)
                           .size(bytes)
                           .usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                           .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                           .build();
        auto map = staging.map();
        std::memcpy(map.as<void>(), data, bytes);
        vmaFlushAllocation(staging.allocator, staging.allocation, 0, bytes);
        map.unmap();
        auto result = vkz::buffer::builder(allocator).size(bytes).usage(usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT).build();
        auto cmd = uploads.create_command_buffer();
        VkBufferCopy copy{0, 0, bytes};
        vkCmdCopyBuffer(cmd, staging, result, 1, &copy);
        vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                     VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
        uploads.submit_and_wait(cmd);
        staging.destroy();
        return result;
    };
    auto vertices = upload(model.vertices.data(), model.vertices.size() * sizeof(vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    std::vector<uint32_t> indices(model.vertices.size());
    for (uint32_t i = 0; i < indices.size(); ++i)
        indices[i] = i;
    auto index_buffer = upload(indices.data(), indices.size() * 4, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
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
    auto caster_buffer = upload(bounds.data(), bounds.size() * sizeof(bounds[0]), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto draw_buffer = upload(templates.data(), templates.size() * sizeof(templates[0]), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto swapchain = app.create_swapchain(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (screenshot.empty() ? 0 : VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
    auto views = vkz::create_swapchain_image_views(device, *swapchain);
    vkz::buffer screenshot_buffer;
    if (!screenshot.empty())
        screenshot_buffer = vkz::buffer::builder(allocator)
                                .size(width * height * 4)
                                .usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                                .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
                                .build();
    vkz::texture depth;
    const auto depth_format = vkz::pick_depth_format(device.physical);
    depth.image = vkz::image::builder(allocator)
                      .format(depth_format)
                      .extent(width, height)
                      .usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                      .build();
    depth.image_view = vkz::image_view::builder(device)
                           .image(depth.image)
                           .format(depth_format)
                           .aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                           .level_count(1)
                           .layer_count(1)
                           .build();
    const auto shadows = vkz::svsm::create({.device = device,
                                            .memory_allocator = allocator,
                                            .physical_pages = 4096,
                                            .max_casters = uint32_t(bounds.size()),
                                            .in_flight_frames = 1,
                                            .first_clipmap_extent = 8,
                                            .fragment_stores_and_atomics_enabled = true});
    auto sky = vkz::texture_from_ktx(sky_path / "skybox_diffuse.ktx", allocator, queue, family, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    auto irradiance =
        vkz::texture_from_ktx(sky_path / "skybox_irradiance.ktx", allocator, queue, family, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    std::vector<vkz::texture> textures(1);
    auto &white = textures.front();
    white.image = vkz::image::builder(allocator)
                      .format(VK_FORMAT_R8G8B8A8_SRGB)
                      .extent(1, 1)
                      .usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                      .build();
    white.image_view = vkz::image_view::builder(device)
                           .image(white.image)
                           .format(VK_FORMAT_R8G8B8A8_SRGB)
                           .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                           .level_count(1)
                           .layer_count(1)
                           .build();
    white.sampler = vkz::sampler::builder(device).build();
    {
        auto cmd = uploads.create_command_buffer();
        vkz::clear(cmd, white.image, VkClearColorValue{{1, 1, 1, 1}}, {});
        vkz::barrier::push_and_flush(cmd, white.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
    std::cout << "Uploaded " << textures.size() << " diffuse textures and Vista skybox" << std::endl;
    auto material_layout = vkz::make_descriptor_set_layout_builder(device)
                               .binding(0)
                               .descriptor_count(1)
                               .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                               .shader_stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                               .binding(1)
                               .descriptor_count(1)
                               .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                               .shader_stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                               .binding(2)
                               .descriptor_count(1)
                               .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                               .shader_stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                               .create_layout();
    vkz::descriptor_pool material_pool{
        device, uint32_t(material_textures.size()), {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, uint32_t(material_textures.size() * 3)}}};
    auto materials = material_pool.allocate_n(material_layout, material_textures.size());
    for (uint32_t i = 0; i < materials.size(); ++i)
        vkz::update_descriptors(
            device, {{materials[i],
                      {vkz::descriptor<vkz::texture_descriptor>(textures[material_textures[i]], 0),
                       vkz::descriptor<vkz::texture_descriptor>(sky, 1), vkz::descriptor<vkz::texture_descriptor>(irradiance, 2)}}});
    vkz::descriptor_set_layout shadow_layout{vkz::svsm::descriptor_set_layout(shadows), device};
    auto prepass =
        make_pipeline(device, VK_FORMAT_UNDEFINED, depth_format, shadow_layout, material_layout, "bistro.vert", "bistro_depth.frag");
    auto forward = make_pipeline(device, swapchain->format(), depth_format, shadow_layout, material_layout, "bistro.vert", "bistro.frag");
    auto shadow = make_pipeline(device, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, shadow_layout, material_layout, "bistro_shadow.vert",
                                "bistro_shadow.frag", true);
    auto sky_pipeline = make_pipeline(device, swapchain->format(), depth_format, shadow_layout, material_layout, "bistro_sky.vert",
                                      "bistro_sky.frag", false, true);
    auto pages_pipeline = make_pipeline(device, swapchain->format(), VK_FORMAT_UNDEFINED, shadow_layout, material_layout,
                                        "bistro_sky.vert", "bistro_pages.frag", false, true);
    vkz::glfw_input_adaptor input(app.window(), false);
    input.bind();
    vkz::camera::camera camera;
    camera.position = {0, 2, 8};
    camera.velocity = {6, 6, 6};
    camera.acceleration = {12, 12, 12};
    camera.rotationSpeed = 0.15f;
    vkz::camera::spectator initializer(camera);
    initializer.look_at(camera.position, {0, 2, -8}, {0, 1, 0});
    initializer.perspective(65, float(width) / height, 0.05f, 2000);
    vkz::camera::controller controller{camera, vkz::camera::movement_type::spectator, input.get_device()};
    vkz::imgui::init({.window = app.window(),
                      .vulkan_context = &context,
                      .queue_family = family,
                      .queue = queue,
                      .min_image_count = 2,
                      .image_count = swapchain->image_count(),
                      .api_version = VK_API_VERSION_1_3,
                      .color_attachment_format = swapchain->format()});
    auto stats_readback = vkz::buffer::builder(allocator)
                              .size(sizeof(vkz::svsm::statistics))
                              .usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                              .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
                              .build();
    auto available = vkz::create_semaphore(device);
    std::vector<VkSemaphore> finished(views.size());
    for (auto &s : finished)
        s = vkz::create_semaphore(device);
    vkz::fenced_command_pools commands{device, queue, family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, 1};
    const test_timeout timeout{argc, argv}; // Timeout starts AFTER loading large assets.
    uint32_t frame = 0;
    int debug = 0;
    float page_size_on_screen = float(height) - 48.0f;
    bool force_uncached = false;
    glm::vec3 sun = glm::normalize(glm::vec3{0.4f, -0.8f, 0.3f});
    vkz::svsm::statistics stats{};
    auto previous = std::chrono::steady_clock::now();
    while (!app.should_close() && !timeout.expired() && (!frame_limit || frame < frame_limit)) {
        app.poll_events();
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;
        controller.process_input();
        controller.update(std::min(dt, 0.1f));
        commands.set_cycle_and_wait(frame);
        if (frame) {
            vmaInvalidateAllocation(stats_readback.allocator, stats_readback.allocation, 0, VK_WHOLE_SIZE);
            auto m = stats_readback.map();
            stats = *m.as<vkz::svsm::statistics>();
            m.unmap();
        }
        uint32_t index;
        VKZ_CHECK_VULKAN(vkAcquireNextImageKHR(device, *swapchain, UINT64_MAX, available, VK_NULL_HANDLE, &index));
        vkz::imgui::new_frame();
        ImGui::SetNextWindowSizeConstraints({390, show_pages ? 420.0f : 220.0f}, {float(width), float(height)});
        ImGui::Begin("Sparse virtual shadows");
        ImGui::Text("Bistro / tinyobjloader | WASD, Q/E, left-drag to look");
        ImGui::Text("%.1f ms | %zu caster batches", dt * 1000, model.batches.size());
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
        ImGui::End();
        auto cmd = commands.create_command_buffer();
        vkz::barrier::push_and_flush(cmd, depth.image, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                     VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                     VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        scene_constants constants{};
        constants.matrix = camera.projection * camera.view;
        constants.light_direction = glm::vec4(glm::normalize(sun), 0);
        constants.info.z = uint32_t(debug);
        auto draw_scene = [&](const vkz::pipeline &pipeline, bool indirect, uint32_t clip) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
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
                vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(constants),
                                   &constants);
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
        vkz::render(cmd, render, [&] { draw_scene(prepass, false, 0); });
        vkz::barrier::push_and_flush(cmd, depth.image, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
                                     VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                     VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        const float depth_extent = glm::length(glm::max(glm::abs(model.minimum), glm::abs(model.maximum))) + 10;
        vkz::svsm::begin(shadows, cmd,
                         {.depth = depth,
                                 .inverse_view_projection = glm::inverse(constants.matrix),
                                 .camera_position = camera.position,
                                 .light_direction = sun,
                                 .light_depth_min = -depth_extent,
                                 .light_depth_max = depth_extent,
                                 .casters = caster_buffer,
                                 .caster_count = uint32_t(bounds.size()),
                                 .indexed_draws = draw_buffer,
                                 .reset_cache = force_uncached});
        auto shadow_set = vkz::svsm::descriptor_set(shadows, 0);
        vkz::svsm::capture(shadows, cmd, [&](const vkz::svsm::render_context &pass) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow.layout, 0, 1, &shadow_set, 0, nullptr);
            draw_scene(shadow, true, pass.clipmap);
        });
        vkz::svsm::end(shadows, cmd);
        vkz::barrier::push_and_flush(cmd, depth.image, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                     VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        auto color = swapchain->get_image(index);
        vkz::barrier::push_and_flush(cmd, color, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_PIPELINE_STAGE_2_NONE,
                                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                     VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        render.color_attachments.push_back({views[index], swapchain->format(), {0, 0, 0, 1}});
        render.depth_attachment->clear = false;
        vkz::render(cmd, render, [&] {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, forward.layout, 0, 1, &shadow_set, 0, nullptr);
            draw_scene(forward, false, 0);
            scene_constants sky_constants{};
            sky_constants.matrix = glm::inverse(camera.projection * camera.view);
            sky_constants.diffuse = glm::vec4(camera.position, 1);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sky_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sky_pipeline.layout, 1, 1, &materials.back().handle, 0, nullptr);
            vkCmdPushConstants(cmd, sky_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(sky_constants), &sky_constants);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        });
        // ImGui's pipeline declares no depth attachment; use a separate color-only rendering scope.
        vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                     VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        render.depth_attachment.reset();
        render.color_attachments[0].clear = false;
        vkz::render(cmd, render, [&] {
            if (show_pages) {
                scene_constants page_constants{};
                page_constants.diffuse = {float(width) - page_size_on_screen - 24.0f, 24.0f, page_size_on_screen, 0};
                page_constants.info.z = uint32_t(page_view);
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pages_pipeline.handle);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pages_pipeline.layout, 0, 1, &shadow_set, 0, nullptr);
                vkCmdPushConstants(cmd, pages_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(page_constants), &page_constants);
                vkCmdDraw(cmd, 3, 1, 0, 0);
            }
            vkz::imgui::render(cmd);
        });
        VkBufferCopy copy{0, 0, sizeof(stats)};
        vkCmdCopyBuffer(cmd, vkz::svsm::counters(shadows, 0), stats_readback, 1, &copy);
        vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                     VK_ACCESS_2_HOST_READ_BIT);
        if (screenshot_buffer._) {
            vkz::barrier::push_and_flush(cmd, color, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                                         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkBufferImageCopy image_copy{};
            image_copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            image_copy.imageExtent = {width, height, 1};
            vkCmdCopyImageToBuffer(cmd, color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, screenshot_buffer, 1, &image_copy);
            vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                                         VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_HOST_READ_BIT);
            vkz::barrier::push_and_flush(cmd, color, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                         VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_TRANSFER_READ_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        } else
            vkz::barrier::push_and_flush(cmd, color, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                                         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_NONE,
                                         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        VKZ_CHECK_VULKAN(vkEndCommandBuffer(cmd));
        commands.enqueue(cmd);
        commands.enqueue_wait(available, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
        commands.enqueue_signal(finished[index]);
        VKZ_CHECK_VULKAN(commands.execute());
        auto handle = swapchain->handle();
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &finished[index];
        present.swapchainCount = 1;
        present.pSwapchains = &handle;
        present.pImageIndices = &index;
        VKZ_CHECK_VULKAN(vkQueuePresentKHR(queue, &present));
        ++frame;
    }
    vkDeviceWaitIdle(device);
    std::cout << "Bistro rendered " << frame << " frames; last pages rendered=" << stats.rendered << ", cached=" << stats.cached
              << ", missing=" << stats.missing << '\n';
    if (screenshot_buffer._ && frame) {
        vmaInvalidateAllocation(screenshot_buffer.allocator, screenshot_buffer.allocation, 0, VK_WHOLE_SIZE);
        auto m = screenshot_buffer.map();
        auto *pixels = m.as<uint8_t>();
        if (swapchain->format() == VK_FORMAT_B8G8R8A8_SRGB || swapchain->format() == VK_FORMAT_B8G8R8A8_UNORM)
            for (uint32_t i = 0; i < width * height; ++i)
                std::swap(pixels[i * 4], pixels[i * 4 + 2]);
        if (!stbi_write_png(screenshot.c_str(), width, height, 4, pixels, width * 4))
            throw std::runtime_error("Could not write screenshot");
        m.unmap();
    }
    screenshot_buffer.destroy();
    vkz::imgui::destroy();
    prepass.destroy();
    forward.destroy();
    shadow.destroy();
    sky_pipeline.destroy();
    pages_pipeline.destroy();
    material_pool.destroy();
    material_layout.destroy();
    for (auto &t : textures)
        t.destroy();
    sky.destroy();
    irradiance.destroy();
    vkz::svsm::destroy(shadows);
    depth.destroy();
    vertices.destroy();
    index_buffer.destroy();
    caster_buffer.destroy();
    draw_buffer.destroy();
    stats_readback.destroy();
    for (auto s : finished)
        vkDestroySemaphore(device, s, nullptr);
    vkDestroySemaphore(device, available, nullptr);
    vkz::destroy_image_views(device, views);
    swapchain.reset();
    allocator.destroy();
    svsm_test::check_validation();
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
