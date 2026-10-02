#define VKZ_IOSTREAM_ADAPTER
#include <vulkanizer/vulkan_app.hpp>
#include <vulkanizer/compute_pipeline_builder.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/status.hpp>
#include "shader_resources.hpp"
#include <cmath>
#include <iostream>
#include <numbers>

int main() try {
    vkz::iostream_adapter::install(std::cout);
    vkz::vulkan_app app{{.width = 32, .height = 32, .title = "glTF BRDF regression", .validation = true}};
    glfwHideWindow(app.window());
    auto &context = app.context();
    auto device = context.device;
    auto allocator = vkz::vma_memory_allocator::create(context);
    auto buffer = vkz::buffer::builder(allocator)
            .size(34 * sizeof(glm::vec4))
            .usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
            .build();
    VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;
    vkz::descriptor_set_layout layout{.device = device};
    VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &layout.handle));
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &size;
    VkDescriptorPool pool;
    VKZ_CHECK_VULKAN(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = pool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &layout.handle;
    VkDescriptorSet set;
    VKZ_CHECK_VULKAN(vkAllocateDescriptorSets(device, &allocate, &set));
    VkDescriptorBufferInfo info{buffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &info;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    auto pipeline = vkz::compute_pipeline_builder{device}.shader_stage().compute_shader(reference::shader("gltf_brdf_test.comp.spv")).layout().add_descriptor_set_layout(layout).build();
    vkz::command_pool commands{device, app.queue_family_index(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, app.graphics_queue()};
    auto cmd = commands.create_command_buffer();
    vkz::bind_pipeline(cmd, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.layout, 0, 1, &set, 0, nullptr);
    vkCmdDispatch(cmd, 1, 1, 1);
    vkz::barrier::push_and_flush(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_HOST_READ_BIT);
    commands.submit_and_wait(cmd);
    vmaInvalidateAllocation(buffer.allocator, buffer.allocation, 0, VK_WHOLE_SIZE);
    auto mapping = buffer.map();
    const auto *values = mapping.as<glm::vec4>();
    auto check = [](bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    };
    for (int i = 0; i < 8; ++i)
        for (int c = 0; c < 3; ++c)
            check(std::isfinite(values[i][c]) && values[i][c] >= 0, "BRDF must remain finite and nonnegative");
    const glm::vec3 base{0.2f, 0.4f, 0.8f};
    for (int c = 0; c < 3; ++c) {
        const float dielectric = (0.96f * base[c] + 0.01f) / std::numbers::pi_v<float>;
        const float metal = base[c] / (4 * std::numbers::pi_v<float>);
        check(std::abs(values[0][c] - dielectric) < 1e-5f, "Dielectric normal-incidence BRDF");
        check(std::abs(values[1][c] - metal) < 1e-5f, "Metal normal-incidence BRDF");
        check(std::abs(values[2][c] - (dielectric + metal) * 0.5f) < 1e-5f, "Metallic mixture");
        check(std::abs(values[3][c] - values[4][c]) < 1e-5f, "BRDF reciprocity");
        check(values[6][c] == 0, "No contribution from lights below the surface");
    }
    check(glm::length(values[8] - glm::vec4{0, 0.04f, 1.0f / 9.0f, 1}) < 1e-6f, "IOR normal-incidence Fresnel");
    check(glm::length(glm::vec3(values[9]) - base / std::numbers::pi_v<float>) < 1e-5f, "IOR one eliminates dielectric reflection");
    check(glm::length(glm::vec3(values[10]) - glm::vec3(0.01f / std::numbers::pi_v<float>)) < 1e-5f, "Transmission removes diffuse, preserves specular");
    check(glm::length(glm::vec3(values[11]) - glm::vec3(0.25f / std::numbers::pi_v<float>)) < 1e-5f, "IOR zero represents infinite IOR");
    check(glm::length(glm::vec3(values[12]) - glm::vec3{0.4f, 0, -std::sqrt(0.84f)}) < 1e-5f, "Refraction obeys Snell's law");
    check(glm::length(values[13] - glm::vec4{0.8f, 0, 0.6f, 1}) < 1e-5f, "Total internal reflection continues reflected ray");
    check(glm::vec3(values[14]) == glm::vec3(0), "Metals do not transmit");
    check(glm::vec3(values[15]) == glm::vec3(0.5f, 0.25f, 0), "Beer absorption including zero color");
    check(glm::length(glm::vec3(values[16]) - glm::vec3(0.25f, 0.0625f, 1)) < 1e-6f, "Absorption scales exponentially with distance");
    check(glm::vec3(values[17]) == glm::vec3(1), "Infinite attenuation distance avoids NaNs for zero color");
    check(glm::length(glm::vec3(values[18]) - glm::vec3(0.6f, 0, -0.8f)) < 1e-5f, "Thin transmission has no macroscopic bending");
    check(std::abs(values[19].x - 0.6f * 1.5f / 1.33f) < 1e-5f && values[19].w == 2, "Nested volume refraction uses incident medium IOR");
    check(values[20].x == 1 && values[20].y == 1.5f, "Leaving nested volume restores outer medium");
    check(glm::length(values[21] - glm::vec4(0.8f, 0, 0.6f, 1)) < 1e-5f, "Total internal reflection retains medium stack");
    check(values[22].x == 0 && std::abs(values[22].y - 0.96f) < 1e-6f, "Leaving outer volume restores air");
    check(glm::length(glm::vec3(values[23]) - base) < 1e-5f && std::abs(values[23].a - 0.4f) < 1e-5f, "Unlit ignores lighting and preserves blend alpha");
    check(glm::length(glm::vec3(values[24]) - glm::vec3(0.02f, 0.01f, 0.005f)) < 1e-5f, "Colored dielectric F0");
    check(glm::length(glm::vec3(values[25]) - glm::vec3(0.5f)) < 1e-5f, "Specular F90 weight");
    for (int i = 26; i <= 32; ++i)
        for (int c = 0; c < 4; ++c)
            check(std::isfinite(values[i][c]), "Extension shading must be finite");
    check(values[26].x > 0.005f, "Clearcoat adds a dielectric lobe");
    check(glm::length(glm::vec3(values[28] - values[29])) > 1e-5f, "Anisotropy rotation changes the lobe");
    check(std::abs(values[30].x - values[30].y) > 1e-4f, "Thin film produces spectral color");
    check(values[31].x > values[32].x && values[31].w < 1.5f && values[32].w > 1.5f, "Dispersion separates RGB refraction");
    check(glm::length(glm::vec2(values[33]) - glm::vec2(2.45f, -0.2f)) < 1e-5f, "Texture transform order");
    mapping.unmap();
    pipeline.destroy();
    vkDestroyDescriptorPool(device, pool, nullptr);
    layout.destroy();
    buffer.destroy();
    allocator.destroy();
    std::cout << "glTF GPU BRDF regressions passed\n";
    return 0;
} catch (const std::exception &error) {

    std::cerr << error.what() << '\n';
    return 1;
}
