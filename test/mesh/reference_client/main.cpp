#define VKZ_IOSTREAM_ADAPTER
#include <vulkanizer/reference.hpp>
#include <vulkanizer/ibl.hpp>
#include <vulkanizer/vulkan_app.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/status.hpp>
#include <iostream>
#include <stdexcept>

int main() try {
    vkz::iostream_adapter::install(std::cout);
    for (auto stage : {vkz::reference::shader_stage::vertex, vkz::reference::shader_stage::fragment, vkz::reference::shader_stage::ray_generation, vkz::reference::shader_stage::miss,
                       vkz::reference::shader_stage::closest_hit, vkz::reference::shader_stage::any_hit, vkz::reference::shader_stage::background_vertex,
                       vkz::reference::shader_stage::background_fragment, vkz::reference::shader_stage::tone_mapping_fragment}) {
        auto words = vkz::reference::shader(stage);
        if (words.empty() || words.front() != 0x07230203) throw std::runtime_error("Missing library SPIR-V");
    }
    vkz::vulkan_app_create_info info{.width = 32, .height = 32, .title = "Installed reference client", .validation = true};
    info.device_extensions = {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                              VK_KHR_RAY_QUERY_EXTENSION_NAME};
    VkPhysicalDeviceBufferDeviceAddressFeatures address{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    address.bufferDeviceAddress = VK_TRUE;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    acceleration.accelerationStructure = VK_TRUE;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR tracing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    tracing.rayTracingPipeline = VK_TRUE;
    VkPhysicalDeviceDescriptorIndexingFeatures indexing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
    indexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    VkPhysicalDeviceRayQueryFeaturesKHR query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
    query.rayQuery = VK_TRUE;
    info.extension_chain.add(query);
    info.extension_chain.add(address).add(acceleration).add(tracing).add(indexing);
    vkz::vulkan_app app{info};
    glfwHideWindow(app.window());
    auto &context = app.context();
    std::array<vkz::descriptor_set_layout, 2> layouts{};
    VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.bindingCount = uint32_t(vkz::reference::scene_bindings.size());
    layout_info.pBindings = vkz::reference::scene_bindings.data();
    layouts[0].device = context.device;
    layouts[1].device = context.device;
    VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &layouts[0].handle));
    layout_info.bindingCount = uint32_t(vkz::reference::material_bindings.size());
    layout_info.pBindings = vkz::reference::material_bindings.data();
    VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &layouts[1].handle));
    vkz::descriptor_set_layout ibl_layout{.device = context.device};
    const auto ibl_bindings = vkz::reference::ibl_bindings(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);
    layout_info.bindingCount = VKZ_COUNT(ibl_bindings);
    layout_info.pBindings = ibl_bindings.data();
    VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &ibl_layout.handle));
    vkz::descriptor_set_layout query_layout{.device = context.device};
    const auto query_bindings = vkz::reference::ray_query_bindings(1);
    layout_info.bindingCount = VKZ_COUNT(query_bindings);
    layout_info.pBindings = query_bindings.data();
    VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &query_layout.handle));
    for (unsigned variant = 0; variant < 16; ++variant) {
        auto pipeline = vkz::reference::create_pipeline({.device = context.device,
                                                         .layouts = layouts,
                                                         .extent = {32, 32},
                                                         .color_format = VK_FORMAT_B8G8R8A8_SRGB,
                                                         .depth_format = vkz::pick_depth_format(context.device.physical),
                                                         .alpha_blend = bool(variant & 2),
                                                         .mirrored = bool(variant & 1),
                                                         .ibl_layout = variant & 4 ? ibl_layout : vkz::descriptor_set_layout{},
                                                         .ray_query_layout = variant & 8 ? query_layout : vkz::descriptor_set_layout{}});
        if (!pipeline.handle || !pipeline.layout) throw std::runtime_error("Missing pipeline handles");
        pipeline.destroy();
    }
    for (auto &layout : layouts)
        layout.destroy();
    auto allocator = vkz::vma_memory_allocator::create(context, VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
    const auto texture_binding = vkz::reference::ray_tracing_texture_binding(1);
    layout_info.bindingCount = VKZ_COUNT(vkz::reference::ray_tracing_scene_bindings);
    layout_info.pBindings = vkz::reference::ray_tracing_scene_bindings.data();
    VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &layouts[0].handle));
    layout_info.bindingCount = 1;
    layout_info.pBindings = &texture_binding;
    VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &layouts[1].handle));
    for (bool ibl : {false, true}) {
        auto ray_pipeline = vkz::reference::create_ray_tracing_pipeline({allocator, layouts, 1, ibl ? ibl_layout : vkz::descriptor_set_layout{}});
        VKZ_ASSERT(ray_pipeline.handle && ray_pipeline.sbt.hit.size && ray_pipeline.sbt.miss.size, "Missing installed ray tracing pipeline");
        ray_pipeline.destroy();
    }
    auto background = vkz::reference::create_background_pipeline({context.device, ibl_layout, {32, 32}, VK_FORMAT_B8G8R8A8_SRGB, vkz::pick_depth_format(context.device.physical)});
    background.destroy();
    {
        vkz::ibl::generator generator{context.device};
        auto lut = vkz::ibl::load_brdf_lut(allocator, app.graphics_queue(), app.queue_family_index());
        lut.destroy();
    }
    vkz::descriptor_set_layout tone_layout{.device = context.device};
    layout_info.bindingCount = 1;
    layout_info.pBindings = &vkz::reference::tone_mapping_binding;
    VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &tone_layout.handle));
    auto tone_pipeline = vkz::reference::create_tone_mapping_pipeline({context.device, tone_layout, {32, 32}, VK_FORMAT_B8G8R8A8_SRGB});
    tone_pipeline.destroy();
    tone_layout.destroy();
    query_layout.destroy();
    ibl_layout.destroy();
    for (auto &layout : layouts)
        layout.destroy();
    allocator.destroy();
    std::cout << "Installed reference shaders and pipelines passed\n";
    return 0;
} catch (const std::exception &e) {

    std::cerr << e.what() << '\n';
    return 1;
}
