#define VKZ_IOSTREAM_ADAPTER
#include "svsm_validation.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/builders.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/svsm.hpp>
#include <vulkanizer/vulkan_app.hpp>

namespace {
    void require(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }
    void upload(vkz::buffer b, const void *data, size_t bytes) {
        auto m = b.map();
        std::memcpy(m.as<void>(), data, bytes);
        vmaFlushAllocation(b.allocator, b.allocation, 0, bytes);
        m.unmap();
    }
} // namespace
int main() try {
    svsm_test::install_logger();
    vkz::vulkan_app app{{.width = 32,
                         .height = 32,
                         .title = "SVSM GPU regression",
                         .validation = true,
                         .enabled_features = {.fragment_stores_and_atomics = true}}};
    glfwHideWindow(app.window());
    auto &context = app.context();
    auto allocator = vkz::vma_memory_allocator::create(context);
    vkz::command_pool commands{context.device, app.queue_family_index(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, app.graphics_queue()};
    auto cpu_buffer = [&](VkDeviceSize size, VkBufferUsageFlags usage) {
        return vkz::buffer::builder(allocator).size(size).usage(usage).memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU).build();
    };
    auto vertices = cpu_buffer(6 * sizeof(glm::vec3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const std::array<glm::vec3, 6> quad{{{-1, -1, 0.25f}, {1, -1, 0.25f}, {1, 1, 0.25f}, {-1, -1, 0.25f}, {1, 1, 0.25f}, {-1, 1, 0.25f}}};
    upload(vertices, quad.data(), sizeof(quad));
    auto indices = cpu_buffer(6 * 4, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    const std::array<uint32_t, 6> index{0, 1, 2, 3, 4, 5};
    upload(indices, index.data(), sizeof(index));
    std::array<vkz::svsm::caster_bounds, 2> bounds{};
    bounds[0].center = {0, 0, 0.25f, 0};
    bounds[0].half_extent = {1, 1, 0, 0};
    bounds[0].metadata.x = 1234567;
    bounds[1].center = {20, 0, 0.25f, 0};
    bounds[1].half_extent = {1, 1, 0, 0};
    auto casters = cpu_buffer(sizeof(bounds), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    upload(casters, bounds.data(), sizeof(bounds));
    auto changes = cpu_buffer(sizeof(bounds), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    upload(changes, bounds.data(), sizeof(bounds));
    const std::array<VkDrawIndexedIndirectCommand, 2> templates{{{6, 1, 0, 0, 0}, {6, 1, 0, 0, 0}}};
    auto draws = cpu_buffer(sizeof(templates), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    upload(draws, templates.data(), sizeof(templates));
    vkz::texture depth;
    depth.image = vkz::image::builder(allocator)
                      .format(VK_FORMAT_R32_SFLOAT)
                      .extent(4, 4)
                      .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                      .build();
    depth.image_view = vkz::image_view::builder(context.device)
                           .image(depth.image)
                           .format(VK_FORMAT_R32_SFLOAT)
                           .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                           .level_count(1)
                           .layer_count(1)
                           .build();
    {
        auto cmd = commands.create_command_buffer();
        vkz::clear(cmd, depth.image, VkClearColorValue{{0.75f, 0, 0, 0}}, {});
        vkz::barrier::push_and_flush(cmd, depth.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        commands.submit_and_wait(cmd);
    }
    vkz::svsm::params config{.device = context.device,
                             .memory_allocator = allocator,
                             .virtual_resolution = 64,
                             .page_size = 16,
                             .clipmap_count = 2,
                             .physical_pages = 32,
                             .max_casters = 2,
                             .in_flight_frames = 2,
                             .max_pages_per_frame = 32,
                             .first_clipmap_extent = 4,
                             .fragment_stores_and_atomics_enabled = true};
    auto shadows = vkz::svsm::create(config);
    auto result = vkz::buffer::builder(allocator)
                      .size(32 * sizeof(glm::vec2))
                      .usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                      .build();
    auto result_layout = vkz::make_descriptor_set_layout_builder(context.device)
                             .binding(0)
                             .descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                             .descriptor_count(1)
                             .shader_stages(VK_SHADER_STAGE_COMPUTE_BIT)
                             .create_layout();
    vkz::descriptor_pool pool{context.device, 1, {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}}};
    auto result_set = pool.allocate(result_layout);
    vkz::update_descriptors(context.device, {{result_set, {vkz::descriptor<vkz::buffer_descriptor>(result, 0)}}});
    auto sample = vkz::make_compute_pipeline_builder(context.device)
                      .shader_stage()
                      .compute_shader(std::string{VKZ_SVSM_SHADER_DIR} + "/svsm_sample.comp.spv")
                      .layout()
                      .add_descriptor_set_layout({vkz::svsm::descriptor_set_layout(shadows), context.device})
                      .add_descriptor_set_layout(result_layout)
                      .build();
    auto readback =
        vkz::buffer::builder(allocator).size(512).usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT).memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU).build();
    vkz::svsm::frame_inputs inputs{.depth = depth,
                                   .light_direction = {0, 0, 1},
                                   .light_depth_min = 0,
                                   .light_depth_max = 1,
                                   .filter_radius = 1,
                                   .casters = casters,
                                   .caster_count = 2,
                                   .indexed_draws = draws};
    uint32_t frame = 0;
    float caster_translation = 0;
    auto run = [&](bool shadowed, bool should_render, bool expect_missing = false) {
        inputs.frame_slot = frame++ % 2;
        auto cmd = commands.create_command_buffer();
        vkz::svsm::begin(shadows, cmd, inputs);
        // Capture the visibility and indirect buffers BEFORE end; they are GPU results.
        vkz::svsm::capture(shadows, cmd, [&](const vkz::svsm::render_context &pass) {
            VkDeviceSize zero = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertices._, &zero);
            vkCmdBindIndexBuffer(cmd, indices, 0, VK_INDEX_TYPE_UINT32);
            for (uint32_t i = 0; i < inputs.caster_count; ++i) {
                vkz::svsm::draw_constants constants{};
                constants.clipmap = pass.clipmap;
                constants.caster_index = i;
                constants.depth_bias = 0;
                constants.model[3].x = caster_translation;
                vkCmdPushConstants(cmd, pass.default_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(constants), &constants);
                vkCmdDrawIndexedIndirect(cmd, pass.indexed_draws, pass.indexed_draw_offset + i * 20, 1, 20);
            }
        });
        vkz::svsm::end(shadows, cmd);
        std::array<VkDescriptorSet, 2> sets{vkz::svsm::descriptor_set(shadows, inputs.frame_slot), result_set.handle};
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sample.handle);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sample.layout, 0, 2, sets.data(), 0, nullptr);
        vkCmdDispatch(cmd, 1, 1, 1);
        vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                     VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        VkBufferCopy region{0, 0, 32 * sizeof(glm::vec2)};
        vkCmdCopyBuffer(cmd, result, readback, 1, &region);
        region = {0, 256, sizeof(vkz::svsm::statistics)};
        vkCmdCopyBuffer(cmd, vkz::svsm::counters(shadows, inputs.frame_slot), readback, 1, &region);
        region = {0, 288, 16};
        vkCmdCopyBuffer(cmd, vkz::svsm::visibility(shadows, inputs.frame_slot), readback, 1, &region);
        region = {0, 304, 80};
        vkCmdCopyBuffer(cmd, vkz::svsm::indexed_draws(shadows, inputs.frame_slot), readback, 1, &region);
        vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                     VK_ACCESS_2_HOST_READ_BIT);
        commands.submit_and_wait(cmd);
        vmaInvalidateAllocation(readback.allocator, readback.allocation, 0, VK_WHOLE_SIZE);
        auto mapped = readback.map();
        const auto *samples = mapped.as<glm::vec2>();
        for (uint32_t i = 0; i < 32; ++i) {
            const float x = (float(i % 4) + 0.5f) * 0.5f - 1;
            const bool covered = i < 16 && shadowed && x > -1 + caster_translation && x < 1 + caster_translation;
            require(samples[i].y == 1 && samples[i].x == (covered ? 0.0f : 1.0f),
                    "Sparse depth sampling differs from dense analytic reference");
        }
        const auto stats = *reinterpret_cast<const vkz::svsm::statistics *>(mapped.as<const char>() + 256);
        require(stats.requested > 0 && (stats.missing > 0) == expect_missing, "Unexpected missing shadow coverage");
        require((stats.rendered > 0) == should_render, "Cache render count incorrect");
        if (inputs.caster_count && should_render && !expect_missing) {
            const auto *visible = reinterpret_cast<const uint32_t *>(mapped.as<const char>() + 288);
            const auto *indirect = reinterpret_cast<const VkDrawIndexedIndirectCommand *>(mapped.as<const char>() + 304);
            require(visible[0] == 1 && visible[1] == 0, "Bounds/page hierarchy culling incorrect");
            require(indirect[0].instanceCount == 1 && indirect[1].instanceCount == 0 && indirect[0].indexCount == 6,
                    "Indirect adapter corrupted draw templates");
        }
        mapped.unmap();
        std::cout << "Frame " << frame << ": requested=" << stats.requested << " rendered=" << stats.rendered << " cached=" << stats.cached
                  << '\n';
    };
    run(true, true);
    run(true, false); // Integer depth rasterization then cache reuse.
    inputs.invalidations = changes;
    inputs.invalidation_count = 1;
    run(true, true); // Redraw unchanged caster after invalidation.
    const auto old_bounds = bounds[0];
    caster_translation = 0.5f;
    bounds[0].center.x = caster_translation;
    std::array<vkz::svsm::invalidation, 2> moved{old_bounds, bounds[0]};
    upload(changes, moved.data(), sizeof(moved));
    upload(casters, bounds.data(), sizeof(bounds));
    inputs.invalidation_count = 2;
    run(true, true); // Old footprint becomes lit, new footprint shadows: both bounds are needed.
    caster_translation = 0;
    bounds[0] = old_bounds;
    upload(casters, bounds.data(), sizeof(bounds));
    run(true, true); // Move back, using the same union of old/new invalidations.
    inputs.caster_count = 0;
    run(false, true); // Deletion clears OLD coverage; no stale shadows.
    inputs.invalidation_count = 0;
    run(false, false);
    inputs.caster_count = 2;
    inputs.reset_cache = true;
    run(true, true);
    inputs.reset_cache = false;
    inputs.camera_position = {0.1f, 0.1f, 0};
    run(true, false); // Sub-page movement preserves cache.
    inputs.camera_position = {-1.1f, 0, 0};
    run(true, false); // Toroidal wrap preserves overlapping pages.
    inputs.reset_cache = true;
    run(true, true); // Retag and rebuild after explicit reset.
    inputs.reset_cache = false;
    inputs.light_depth_max = 2;
    run(true, true); // Depth normalization changes invalidate.
    inputs.light_direction = {0.1f, 0, 1};
    run(true, true); // Light rotation invalidates.
    // A four-page pool can hold only the coarse coverage. Every fine lookup must safely fall back.
    vkz::svsm::destroy(shadows);
    config.physical_pages = 4;
    config.max_pages_per_frame = 4;
    shadows = vkz::svsm::create(config);
    inputs.camera_position = {0, 0, 0};
    inputs.light_direction = {0, 0, 1};
    inputs.light_depth_max = 1;
    run(true, true, true);
    run(true, false, true);
    // A separate budget constraint has the same fallback contract even with ample physical memory.
    vkz::svsm::destroy(shadows);
    config.physical_pages = 32;
    config.max_pages_per_frame = 4;
    shadows = vkz::svsm::create(config);
    run(true, true, true);
    run(true, true, false);
    run(true, false, false);
    std::vector<VkCommandBuffer> in_flight;
    for (uint32_t slot = 0; slot < 2; ++slot) {
        auto cmd = commands.create_command_buffer();
        inputs.frame_slot = slot;
        vkz::svsm::begin(shadows, cmd, inputs);
        vkz::svsm::capture(shadows, cmd, [](const auto &) {}); // All pages already cached.
        vkz::svsm::end(shadows, cmd);
        in_flight.push_back(cmd);
    }
    commands.submit_and_wait(in_flight); // Both slots recorded before either submission completes.
    run(true, false, false);
    auto abandoned = commands.create_command_buffer();
    vkz::svsm::begin(shadows, abandoned, inputs);
    commands.destroy(abandoned);
    vkz::svsm::reset(shadows);
    run(true, true, true); // Recover discarded recording, including page/pool state.
    run(true, true, false);
    vkDeviceWaitIdle(context.device);
    sample.destroy();
    pool.destroy();
    result_layout.destroy();
    result.destroy();
    readback.destroy();
    vkz::svsm::destroy(shadows);
    for (auto *b : {&vertices, &indices, &casters, &changes, &draws})
        b->destroy();
    depth.destroy();
    allocator.destroy();
    svsm_test::check_validation();
    std::cout << "SVSM GPU regression passed: uint depth, PCF, cache, invalidation, deletion, wrapping and indirect culling\n";
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
