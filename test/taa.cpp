#define VKZ_IOSTREAM_ADAPTER
#include <vulkanizer/taa.hpp>
#include <vulkanizer/vulkan_app.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/log.hpp>
#include <glm/gtc/packing.hpp>
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    constexpr VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    void readable(VkCommandBuffer cmd, vkz::image &image) {
        vkz::barrier::push_and_flush(cmd, image, range, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                                     VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void require(bool condition, const char *message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }
} // namespace

int main() {
    vkz::iostream_adapter::install(std::cout);
    vkz::vulkan_app app{{.width = 32, .height = 32, .title = "TAA regression", .validation = true}};
    glfwHideWindow(app.window());
    auto &context = app.context();
    auto allocator = vkz::vma_memory_allocator::create(context);
    vkz::command_pool commands{context.device, app.queue_family_index(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, app.graphics_queue()};
    auto target = [&](VkFormat format, uint32_t width, uint32_t height) {
        vkz::texture texture{};
        texture.image =
            vkz::image::builder(allocator)
                .format(format)
                .extent(width, height)
                .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .build();
        texture.image_view = vkz::image_view::builder(context.device)
            .image(texture.image)
            .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
            .level_count(1)
            .layer_count(1)
            .build();
        return texture;
    };
    for (const auto format : {VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT}) {
        auto color = target(format, 13, 9);
        auto depth = target(VK_FORMAT_R32_SFLOAT, 13, 9);
        auto initialize = [&] {
            auto cmd = commands.create_command_buffer();
            vkz::clear(cmd, color.image, VkClearColorValue{{0, 0, 0, 1}}, vkz::sub_resource{});
            vkz::clear(cmd, depth.image, VkClearColorValue{{0.5f, 0, 0, 0}}, vkz::sub_resource{});
            readable(cmd, color.image);
            readable(cmd, depth.image);
            commands.submit_and_wait(cmd);
        };
        initialize();
        const auto taa = vkz::taa::create({.device = context.device, .memory_allocator = allocator, .color = color, .depth = depth});
        auto readback = vkz::buffer::builder(allocator)
            .size(17 * 11 * 16)
            .usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
            .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
            .build();
        uint32_t frame = 0;
        auto run = [&](glm::vec4 value, glm::vec4 expected, vkz::taa::camera camera = {}) {
            vkz::taa::update(taa, camera, frame++);
            auto cmd = commands.create_command_buffer();
            vkz::clear(cmd, color.image, VkClearColorValue{{value.x, value.y, value.z, value.w}}, vkz::sub_resource{});
            readable(cmd, color.image);
            vkz::taa::resolve(taa, cmd);
            vkz::copy(cmd, color.image, readback);
            readable(cmd, color.image);
            vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_HOST_READ_BIT);
            commands.submit_and_wait(cmd);
            auto mapping = readback.map();
            const auto count = color.image.create_info.extent.width * color.image.create_info.extent.height;
            for (uint32_t i = 0; i < count; ++i) {
                glm::vec4 pixel;
                if (format == VK_FORMAT_R32G32B32A32_SFLOAT) {
                    pixel = mapping.as<const glm::vec4>()[i];
                } else {
                    const auto *packed = mapping.as<const uint32_t>() + i * 2;
                    pixel = glm::vec4{glm::unpackHalf2x16(packed[0]), glm::unpackHalf2x16(packed[1])};
                }
                for (int c = 0; c < 4; ++c) {
                    require(std::isfinite(pixel[c]) && std::abs(pixel[c] - expected[c]) < 0.003f, "Unexpected TAA pixel (history/filter/reset/edge)");
                }
            }
            mapping.unmap();
        };
        const glm::vec4 black{0, 0, 0, 0.75f};
        const glm::vec4 white{1, 1, 1, 0.75f};
        run(black, black);
        run(black, black, {.jitter = vkz::taa::jitter(1)});
        vkz::taa::settings simple{.full_taa = false, .history_weight = 0.5f};
        vkz::taa::configure(taa, simple);
        run(black, black);
        run(white, glm::vec4{0.5f, 0.5f, 0.5f, 0.75f});
        vkz::taa::reset(taa);
        run(white, white);
        glm::mat4 moved{1};
        moved[3][0] = 0.25f;
        run(white, white, {.view_projection = moved});
        auto motion = vkz::taa::motion_vectors(taa);
        auto cmd = commands.create_command_buffer();
        vkz::copy(cmd, motion.image, readback);
        vkz::barrier::push_and_flush(cmd, motion.image, range, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                     VK_IMAGE_LAYOUT_GENERAL);
        vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_HOST_READ_BIT);
        commands.submit_and_wait(cmd);
        auto mapping = readback.map();
        const auto velocity = glm::unpackHalf2x16(mapping.as<const uint32_t>()[0]);
        require(std::abs(velocity.x - 0.125f) < 0.001f && std::abs(velocity.y) < 0.001f, "Motion vectors must use UV units");
        mapping.unmap();
        for (uint32_t filter = 0; filter < 4; ++filter) {
            for (uint32_t constraint = 0; constraint < 5; ++constraint) {
                vkz::taa::settings options{};
                options.sample_filter = vkz::taa::sub_sample_filter(filter);
                options.constraint = vkz::taa::history_constraint(constraint);
                vkz::taa::configure(taa, options);
                run(black, black);
                run(black, black, {.jitter = vkz::taa::jitter(frame)});
                vkz::taa::reset(taa);
                run(white, white);
                run(white, white, {.jitter = vkz::taa::jitter(frame)});
            }
        }
        vkz::taa::configure(taa, simple);
        run(black, black);
        auto depth_cmd = commands.create_command_buffer();
        vkz::clear(depth_cmd, depth.image, VkClearColorValue{{0.25f, 0, 0, 0}}, vkz::sub_resource{});
        readable(depth_cmd, depth.image);
        commands.submit_and_wait(depth_cmd);
        run(white, white); // Newly exposed depth must reject the black history.
        moved[3][0] = 4.0f;
        run(black, black, {.view_projection = moved}); // Offscreen history must be rejected.
        auto resized_color = target(format, 17, 11);
        auto resized_depth = target(VK_FORMAT_R32_SFLOAT, 17, 11);
        color.image_view.destroy();
        color.image.destroy();
        depth.image_view.destroy();
        depth.image.destroy();
        color = resized_color;
        depth = resized_depth;
        initialize();
        vkz::taa::resize(taa, color, depth);
        run(white, white);
        run(white, white);
        vkz::taa::destroy(taa);
        readback.destroy();
        color.image_view.destroy();
        color.image.destroy();
        depth.image_view.destroy();
        depth.image.destroy();
    }
    allocator.destroy();
    std::cout << "TAA GPU regression passed: RGBA16F/32F, history blending/reset, filters, motion, odd extents and resize\n";
}
