#define VKZ_IOSTREAM_ADAPTER
#include <vulkanizer/reference.hpp>
#include <vulkanizer/vulkan_app.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/descriptors.hpp>
#include <vulkanizer/render.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/status.hpp>
#include <array>
#include <cmath>
#include <iostream>

namespace {
    vkz::texture make_texture(vkz::vma_memory_allocator &allocator, VkFormat format, VkImageUsageFlags usage) {
        vkz::texture texture;
        texture.image = vkz::image::builder(allocator)
            .format(format)
            .extent(1, 1)
            .usage(usage)
            .build();
        texture.image_view = vkz::image_view::builder(allocator.device)
            .image(texture.image)
            .format(format)
            .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
            .level_count(1)
            .layer_count(1)
            .build();
        texture.sampler = vkz::sampler::builder(allocator.device)
            .min_filter(VK_FILTER_NEAREST)
            .mag_filter(VK_FILTER_NEAREST)
            .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
            .max_lod(0)
            .build();
        return texture;
    }

    struct sample {
        glm::vec4 hdr;
        float exposure;
        std::array<int, 4> expected;
    };
}

int main() try {
    vkz::iostream_adapter::install(std::cout);
    vkz::vulkan_app app{{.width = 32, .height = 32, .title = "Tone mapping regression", .validation = true}};
    glfwHideWindow(app.window());
    const auto device = app.context().device;
    auto allocator = vkz::vma_memory_allocator::create(app.context());
    auto input = make_texture(allocator, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    auto output = make_texture(allocator, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    auto readback = vkz::buffer::builder(allocator)
        .size(4)
        .usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
        .build();
    vkz::descriptor_set_layout layout{.device = device};
    VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.bindingCount = 1;
    layout_info.pBindings = &vkz::reference::tone_mapping_binding;
    VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &layout.handle));
    vkz::descriptor_pool pool{device, 1, {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}}};
    const auto set = pool.allocate(layout);
    vkz::update_descriptors(device, {{set, {vkz::texture_descriptor{input.image_view, input.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0}}}});
    auto pipeline = vkz::reference::create_tone_mapping_pipeline({device, layout, {1, 1}, VK_FORMAT_R8G8B8A8_SRGB});
    pipeline.descriptor_sets = {set};
    const std::array<sample, 4> samples{
        {{{0.18f, 1, 2, 0.25f}, 0, {141, 232, 245, 64}}, {{0, 4, 100, 0.75f}, 0, {0, 252, 255, 191}}, {{-1, 0.5f, 2, 1}, 1, {0, 232, 252, 255}}, {{0.36f, 2, 4, 0.5f}, -1, {141, 232, 245, 128}}}};
    vkz::command_pool commands{device, app.queue_family_index(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, app.graphics_queue()};
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    for (const auto &sample : samples) {
        const auto cmd = commands.create_command_buffer();
        vkz::clear(cmd, input.image, VkClearColorValue{{sample.hdr.r, sample.hdr.g, sample.hdr.b, sample.hdr.a}}, {});
        vkz::barrier::push_and_flush(cmd, input.image, range, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                     VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkz::barrier::push_and_flush(cmd, output.image, range, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkz::render_info rendering;
        rendering.render_area = {1, 1};
        rendering.color_attachments.push_back({.view = output.image_view, .format = VK_FORMAT_R8G8B8A8_SRGB});
        vkz::render(cmd, rendering, [&] {
            vkz::bind_pipeline(cmd, pipeline);
            vkz::push_constants(cmd, pipeline, vkz::reference::tone_mapping_uniform{sample.exposure}, VK_SHADER_STAGE_FRAGMENT_BIT);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        });
        vkz::barrier::push_and_flush(cmd, output.image, range, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                     VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {1, 1, 1};
        vkCmdCopyImageToBuffer(cmd, output.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
        vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_HOST_READ_BIT);
        commands.submit_and_wait(cmd);
        VKZ_CHECK_VULKAN(vmaInvalidateAllocation(readback.allocator, readback.allocation, 0, VK_WHOLE_SIZE));
        auto mapping = readback.map();
        const auto *pixel = mapping.as<uint8_t>();
        for (size_t c = 0; c < 4; ++c) {
            VKZ_ASSERT(std::abs(int(pixel[c]) - sample.expected[c]) <= 2, "Tone mapping must preserve HDR highlights, apply exposure before the curve and encode sRGB exactly once");
        }
        mapping.unmap();
    }
    pipeline.destroy();
    pool.destroy();
    layout.destroy();
    input.destroy();
    output.destroy();
    readback.destroy();
    allocator.destroy();
    std::cout << "Tone mapping HDR highlights, exposure, alpha and sRGB encoding passed\n";
    return 0;
} catch (const std::exception &error) {

    std::cerr << error.what() << '\n';
    return 1;
}
