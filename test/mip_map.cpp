#define VKZ_IOSTREAM_ADAPTER

#include <vulkanizer/barrier.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/memory.hpp>
#include <vulkanizer/mip_map.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/vulkan_app.hpp>

#include <vulkanizer/log.hpp>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
    constexpr uint32_t image_size = 2048;
    constexpr uint32_t tile_size = 16;
    constexpr uint32_t mip_levels = 12;

    std::vector<uint32_t> checkerboard() {
        std::vector<uint32_t> pixels(image_size * image_size);
        for (uint32_t y = 0; y < image_size; ++y) {
            for (uint32_t x = 0; x < image_size; ++x) {
                const bool white = ((x / tile_size) + (y / tile_size)) % 2 == 0;
                pixels[y * image_size + x] = white ? 0xffffffffu : 0xff000000u;
            }
        }
        return pixels;
    }
}

int main() {
    vkz::iostream_adapter::install(std::cout);

    vkz::vulkan_app app{{64, 64, "vulkanizer mip map test"}};
    auto& context = app.context();
    auto allocator = vkz::vma_memory_allocator::create(context);
    const auto family = app.queue_family_index();
    const auto queue = app.graphics_queue();

    auto pixels = checkerboard();
    const VkDeviceSize image_bytes = pixels.size() * sizeof(pixels.front());
    auto staging = vkz::buffer::builder(allocator)
        .size(image_bytes)
        .usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
        .memory_usage(VMA_MEMORY_USAGE_CPU_ONLY)
        .build();
    {
        auto mapping = staging.map();
        std::memcpy(mapping.as<void>(), pixels.data(), static_cast<size_t>(image_bytes));
        mapping.unmap();
    }

    auto image = vkz::image::builder(allocator)
        .format(VK_FORMAT_R8G8B8A8_UNORM)
        .extent(image_size, image_size)
        .mip_levels(mip_levels)
        .usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT)
        .build();
    auto readback = vkz::buffer::builder(allocator)
        .size(sizeof(uint32_t))
        .usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
        .build();

    vkz::command_pool commands{
        context.device, family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue};
    const auto command_buffer = commands.create_command_buffer();
    const VkImageSubresourceRange all_mips{
        VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, 1};
    vkz::barrier::push_and_flush(
        command_buffer, image.handle, all_mips,
        VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    image.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    vkz::copy(command_buffer, staging, image);
    vkz::generate_mip_maps(command_buffer, image);
    commands.submit_and_wait(command_buffer);


    const auto readback_command_buffer = commands.create_command_buffer();

    const VkImageSubresourceRange final_mip{
        VK_IMAGE_ASPECT_COLOR_BIT, mip_levels - 1, 1, 0, 1};
    auto final_mip_view = vkz::image_view::builder(context.device)
        .image(image)
        .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
        .base_mip_level(mip_levels - 1)
        .level_count(1)
        .layer_count(1)
        .build();
    vkz::barrier::push_and_flush(
        readback_command_buffer, image.handle, final_mip,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
        image.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    image.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    vkz::copy(readback_command_buffer, image, final_mip_view, readback);
    vkz::barrier::push_and_flush(
        readback_command_buffer,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_HOST_READ_BIT);

    // This reaches vkQueueWaitIdle and reproduces a device loss caused by invalid mip commands.
    commands.submit_and_wait(readback_command_buffer);

    auto mapping = readback.map();
    const auto pixel = *mapping.as<const uint32_t>();
    mapping.unmap();
    const auto red = pixel & 0xffu;
    const auto green = (pixel >> 8) & 0xffu;
    const auto blue = (pixel >> 16) & 0xffu;
    const auto alpha = pixel >> 24;
    assert(red >= 120 && red <= 135);
    assert(green >= 120 && green <= 135);
    assert(blue >= 120 && blue <= 135);
    assert(alpha == 255);

    readback.destroy();
    final_mip_view.destroy();
    image.destroy();
    staging.destroy();
    allocator.destroy();
}
