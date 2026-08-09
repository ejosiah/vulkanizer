#define VKZ_IOSTREAM_ADAPTER

#include <vulkanizer/vulkan_app.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/descriptor_pool.hpp>
#include <vulkanizer/descriptor_set_builder.hpp>
#include <vulkanizer/graphics_pipeline_builder.hpp>
#include <vulkanizer/imgui.hpp>
#include <vulkanizer/io.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/memory.hpp>
#include <vulkanizer/render.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/glfw_input_adaptor.hpp>

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

#ifdef _WIN32
#include <wincodec.h>
#include <windows.h>
#endif

#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    constexpr uint32_t width = 1280;
    constexpr uint32_t height = 800;

    struct camera_constants {
        glm::mat4 model{1};
        glm::mat4 view{1};
        glm::mat4 projection{1};
    };

    struct quad_vertex {
        glm::vec2 position;
        glm::vec2 uv;
    };

    struct pixels {
        uint32_t width{};
        uint32_t height{};
        std::vector<uint8_t> rgba;
    };

    std::string shader_path(const char* name) {
        return std::string{VKZ_CAMERA_TEST_SHADER_DIR} + "/" + name + ".spv";
    }

#ifdef _WIN32
    template<class T> struct com_deleter { void operator()(T* value) const { if (value) value->Release(); } };

    pixels load_rgba(const std::string& filename) {
        const auto wide_size = MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, nullptr, 0);
        std::wstring wide(static_cast<size_t>(wide_size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, wide.data(), wide_size);

        IWICImagingFactory* factory_raw{};
        const auto hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(&factory_raw));
        if (FAILED(hr)) throw std::runtime_error("Could not create WIC image factory");
        std::unique_ptr<IWICImagingFactory, com_deleter<IWICImagingFactory>> factory(factory_raw);

        IWICBitmapDecoder* decoder_raw{};
        if (FAILED(factory->CreateDecoderFromFilename(wide.c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnLoad, &decoder_raw))) throw std::runtime_error("Could not load " + filename);
        std::unique_ptr<IWICBitmapDecoder, com_deleter<IWICBitmapDecoder>> decoder(decoder_raw);
        IWICBitmapFrameDecode* frame_raw{};
        decoder->GetFrame(0, &frame_raw);
        std::unique_ptr<IWICBitmapFrameDecode, com_deleter<IWICBitmapFrameDecode>> frame(frame_raw);
        IWICFormatConverter* converter_raw{};
        factory->CreateFormatConverter(&converter_raw);
        std::unique_ptr<IWICFormatConverter, com_deleter<IWICFormatConverter>> converter(converter_raw);
        converter->Initialize(frame.get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                              nullptr, 0, WICBitmapPaletteTypeCustom);
        pixels result;
        converter->GetSize(&result.width, &result.height);
        result.rgba.resize(static_cast<size_t>(result.width) * result.height * 4);
        converter->CopyPixels(nullptr, result.width * 4, static_cast<UINT>(result.rgba.size()), result.rgba.data());
        return result;
    }
#else
    pixels load_rgba(const std::string&) { throw std::runtime_error("The camera test JPEG loader currently requires WIC"); }
#endif

    VkPipeline make_pipeline(vkz::context& context, VkFormat color, VkFormat depth,
            const char* vert, const char* frag, VkDescriptorSetLayout descriptors, VkPipelineLayout& layout) {
        vkz::graphics_pipeline_builder pipeline_builder{context.device};
        pipeline_builder.shader_stage().vertex_shader(shader_path(vert)).fragment_shader(shader_path(frag))
            .vertex_input_state()
                .add_vertex_binding_description(0, sizeof(quad_vertex), VK_VERTEX_INPUT_RATE_VERTEX)
                .add_vertex_attribute_description(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(quad_vertex, position))
                .add_vertex_attribute_description(1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(quad_vertex, uv))
            .input_assembly_state().triangles()
            .viewport_state().viewport().origin(0, 0).dimension(width, height).min_depth(0).max_depth(1)
                .scissor().offset(0, 0).extent(width, height)
            .rasterization_state().cull_none().front_face_counter_clockwise().polygon_mode_fill()
            .multisample_state().rasterization_samples(VK_SAMPLE_COUNT_1_BIT)
            .depth_stencil_state().enable_depth_test().enable_depth_write().compare_op_less_or_equal()
            .color_blend_state().attachment().add();
        auto& builder = pipeline_builder.layout();
        if (descriptors) builder.add_descriptor_set_layout(descriptors);
        return builder.add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(camera_constants))
            .dynamic_render_pass().add_color_attachment(color).depth_attachment(depth).build(layout);
    }
}

int main() {
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
    vkz::iostream_adapter::install(std::cout);
    vkz::vulkan_app app{{
        .width = width,
        .height = height,
        .title = "vulkanizer camera test",
        .resizable = false,
    }};
    auto& context = app.context();
    auto allocator = vkz::vma_memory_allocator::create(context);
    const auto device = context.device.logical;
    const auto queue = app.graphics_queue();
    const auto family = app.queue_family_index();

    vkz::glfw_input_adaptor input(app.window());
    input.bind();
    vkz::camera::camera camera;
    camera.eyes = {0, 1, 5};
    camera.acceleration = {18, 18, 18};
    camera.velocity = {8, 8, 8};
    camera.rotationSpeed = 0.15f;
    camera.horizontal_fov = true;
    vkz::camera::spectator initializer(camera);
    initializer.look_at(camera.eyes, {0, 0, 0}, {0, 1, 0});
    initializer.perspective(65, float(width) / height, 0.05f, 1000);
    auto movement = vkz::camera::movement_type::spectator;
    auto controller = std::make_unique<vkz::camera::controller>(camera, movement, input.get_device());

    auto swapchain = app.create_swapchain();
    auto swap_views = vkz::create_swapchain_image_views(device, *swapchain);
    const auto depth_format = vkz::pick_depth_format(context.device.physical);
    auto depth_image = vkz::image::builder(allocator).format(depth_format).extent(width, height)
        .usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT).build();
    auto depth_view = vkz::image_view::builder(device).image(depth_image).view_type(VK_IMAGE_VIEW_TYPE_2D)
        .format(depth_format).aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT).level_count(1).layer_count(1).build();

    const std::array<const char*, 6> face_names{"right.jpg", "left.jpg", "top.jpg", "bottom.jpg", "front.jpg", "back.jpg"};
    std::vector<pixels> faces;
    for (const auto* name : face_names) faces.push_back(load_rgba(std::string{VKZ_CAMERA_TEST_SKYBOX_DIR} + "/" + name));
    for (const auto& face : faces) if (face.width != faces[0].width || face.height != faces[0].height)
        throw std::runtime_error("All skybox faces must have identical dimensions");
    const auto face_size = faces[0].rgba.size();
    std::vector<uint8_t> cube_pixels(face_size * faces.size());
    for (size_t i = 0; i < faces.size(); ++i) std::memcpy(cube_pixels.data() + i * face_size, faces[i].rgba.data(), face_size);
    vkz::staging_buffer staging{allocator, cube_pixels.size()};
    auto staging_memory = staging.borrow(cube_pixels.size());
    staging_memory.copy_from(cube_pixels.data());
    auto cube_image = vkz::image::builder(allocator).flags(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
        .format(VK_FORMAT_R8G8B8A8_SRGB).extent(faces[0].width, faces[0].height).array_layers(6)
        .usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT).build();
    {
        vkz::scope_command_buffer upload{device, family, queue};
        VkImageSubresourceRange cube_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
        VkImage cube_handle = cube_image;
        vkz::barrier::push_and_flush(upload, cube_handle, cube_range, VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        std::array<VkBufferImageCopy, 6> copies{};
        for (uint32_t i = 0; i < 6; ++i) {
            copies[i].bufferOffset = staging_memory.offset() + face_size * i;
            copies[i].imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, i, 1};
            copies[i].imageExtent = {faces[0].width, faces[0].height, 1};
        }
        vkCmdCopyBufferToImage(upload, staging.handle(), cube_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, copies.data());
        vkz::barrier::push_and_flush(upload, cube_handle, cube_range, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    staging.return_memory(staging_memory);
    staging.destroy();
    auto cube_view = vkz::image_view::builder(device).image(cube_image).view_type(VK_IMAGE_VIEW_TYPE_CUBE)
        .format(VK_FORMAT_R8G8B8A8_SRGB).aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT).level_count(1).layer_count(6).build();
    auto cube_sampler = vkz::sampler::builder(device).mag_filter(VK_FILTER_LINEAR).min_filter(VK_FILTER_LINEAR)
        .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE).build();

    auto descriptor_layout = vkz::descriptor_set_layout_builder{context.device}.binding(0).descriptor_count(1)
        .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER).shader_stages(VK_SHADER_STAGE_FRAGMENT_BIT).create_layout();
    vkz::descriptor_pool descriptor_pool{
        context.device,
        1,
        {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}},
    };
    auto descriptor = descriptor_pool.allocate(descriptor_layout);
    VkDescriptorImageInfo image_info{cube_sampler, cube_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = descriptor; write.descriptorCount = 1; write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &image_info;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    VkPipelineLayout sky_layout{}, floor_layout{};
    auto sky_pipeline = make_pipeline(context, swapchain->format(), depth_format, "camera_skybox.vert", "camera_skybox.frag", descriptor_layout, sky_layout);
    auto floor_pipeline = make_pipeline(context, swapchain->format(), depth_format, "camera_floor.vert", "camera_floor.frag", {}, floor_layout);
    constexpr std::array quad_vertices{
        quad_vertex{{-1, -1}, {0, 0}}, quad_vertex{{ 1, -1}, {1, 0}}, quad_vertex{{ 1,  1}, {1, 1}},
        quad_vertex{{-1, -1}, {0, 0}}, quad_vertex{{ 1,  1}, {1, 1}}, quad_vertex{{-1,  1}, {0, 1}},
    };
    auto quad_buffer = vkz::buffer::builder(allocator).size(sizeof(quad_vertices))
        .usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT).memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU).build();
    {
        auto mapped = quad_buffer.map();
        std::memcpy(mapped.as<void>(), quad_vertices.data(), sizeof(quad_vertices));
        mapped.unmap();
    }
    vkz::imgui::init({.window = app.window(), .vulkan_context = &context, .queue_family = family, .queue = queue,
        .min_image_count = 2, .image_count = swapchain->image_count(), .api_version = VK_API_VERSION_1_3,
        .color_attachment_format = swapchain->format()});
    auto available = vkz::create_semaphore(device), finished = vkz::create_semaphore(device);
    vkz::fenced_command_pools commands{device, queue, family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, 1};
    uint32_t frame{};
    auto previous = std::chrono::steady_clock::now();

    while (!app.should_close()) {
        app.poll_events();
        input.process_game_pad_input();
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count(); previous = now;
        controller->process_input();
        controller->update(dt);
        uint32_t index{}; VKZ_CHECK_VULKAN(vkAcquireNextImageKHR(device, swapchain->handle(), UINT64_MAX, available, {}, &index));
        vkz::imgui::new_frame();
        ImGui::Begin("Camera");
        int selected = movement == vkz::camera::movement_type::spectator ? 0 : 1;
        if (ImGui::Combo("Movement", &selected, "Spectator\0First person\0")) {
            movement = selected == 0 ? vkz::camera::movement_type::spectator : vkz::camera::movement_type::first_person;
            controller = std::make_unique<vkz::camera::controller>(camera, movement, input.get_device());
        }
        ImGui::Text("WASD move, Q/E down/up, hold left mouse to look, wheel zoom");
        ImGui::Text("Position %.2f, %.2f, %.2f", camera.eyes.x, camera.eyes.y, camera.eyes.z);
        ImGui::End();
        camera_constants constants{camera.model, camera.view, camera.projection};

//        constants.view = glm::lookAt({0, 1, 5}, glm::vec3{0}, {0, 1, 0});
//        constants.projection = glm::perspective(glm::radians(65.0f), camera.aspect_ratio, 0.1f, 100.f);
        commands.set_cycle_and_wait(frame++);
        auto cmd = commands.create_command_buffer(); VkImage color = swapchain->get_image(index);
        VkImageSubresourceRange color_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkz::barrier::push_and_flush(cmd, color, color_range, VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkImage depth_handle = depth_image; VkImageSubresourceRange depth_range{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        vkz::barrier::push_and_flush(cmd, depth_handle, depth_range, VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_NONE, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        vkz::render_info rendering{};
        rendering.color_attachments.push_back({.view=swap_views[index], .format=swapchain->format(), .clear_value={0,0,0,1}});
        rendering.depth_attachment = vkz::depth_stencil_attachment{.view=depth_view, .format=depth_format, .clear_value={1,0}};
        rendering.render_area = {width, height};
        vkz::render(cmd, rendering, [&] {
            const VkDeviceSize vertex_offset = 0;
            VkBuffer vertex_buffer = quad_buffer;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &vertex_offset);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sky_pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sky_layout, 0, 1, &descriptor, 0, nullptr);
            vkCmdPushConstants(cmd, sky_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(constants), &constants);
            vkCmdDraw(cmd, static_cast<uint32_t>(quad_vertices.size()), 1, 0, 0);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, floor_pipeline);
            vkCmdPushConstants(cmd, floor_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(constants), &constants);
            vkCmdDraw(cmd, static_cast<uint32_t>(quad_vertices.size()), 1, 0, 0); vkz::imgui::render(cmd);
        });
        vkz::barrier::push_and_flush(cmd, color, color_range, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        VKZ_CHECK_VULKAN(vkEndCommandBuffer(cmd));
        commands.enqueue(cmd);
        commands.enqueue_wait(available, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        commands.enqueue_signal(finished);
        VKZ_CHECK_VULKAN(commands.execute());
        const auto handle = swapchain->handle(); VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount=1; present.pWaitSemaphores=&finished; present.swapchainCount=1; present.pSwapchains=&handle; present.pImageIndices=&index;
        VKZ_CHECK_VULKAN(vkQueuePresentKHR(queue, &present));
    }
    vkDeviceWaitIdle(device);
    vkz::imgui::destroy(); vkDestroyPipeline(device, floor_pipeline, nullptr); vkDestroyPipeline(device, sky_pipeline, nullptr);
    vkDestroyPipelineLayout(device, floor_layout, nullptr); vkDestroyPipelineLayout(device, sky_layout, nullptr);
    descriptor_pool.destroy(); vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
    quad_buffer.destroy(); cube_sampler.destroy(); cube_view.destroy(); cube_image.destroy();
    depth_view.destroy(); depth_image.destroy(); vkz::destroy_image_views(device, swap_views);
    vkDestroySemaphore(device, finished, nullptr); vkDestroySemaphore(device, available, nullptr);
    swapchain.reset(); allocator.destroy();
#ifdef _WIN32
    CoUninitialize();
#endif
}
