#define VKZ_IOSTREAM_ADAPTER
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <vulkanizer/ibl.hpp>
#include <vulkanizer/ktx.hpp>
#include <vulkanizer/vulkan_app.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/status.hpp>
#include <cmath>
#include <cstring>
#include <iostream>
#include <numbers>
#include <chrono>
#include <array>

namespace {
    struct temporary_hdr {
        std::filesystem::path path = std::filesystem::temp_directory_path() / ("vkz_ibl_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".hdr");

        ~temporary_hdr() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    };

    void check_constant(const vkz::ktx::texture_snapshot &snapshot, glm::vec3 expected) {
        VKZ_ASSERT(snapshot.create_info.format == VK_FORMAT_R32G32B32A32_SFLOAT, "IBL preserves floating-point radiance");
        for (size_t offset = 0; offset < snapshot.pixels.size(); offset += sizeof(glm::vec4)) {
            glm::vec4 value;
            std::memcpy(&value, snapshot.pixels.data() + offset, sizeof(value));
            for (int c = 0; c < 4; ++c) {
                const auto target = c == 3 ? 1.0f : expected[c];
                VKZ_ASSERT(std::isfinite(value[c]) && std::abs(value[c] - target) < 0.001f, "Constant environment must retain its energy across all faces and mips");
            }
        }
    }

    void check_axes(const vkz::ktx::texture_snapshot &snapshot, bool irradiance) {
        const std::array<glm::vec3, 6> axes{glm::vec3{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        const auto size = snapshot.create_info.extent.width;
        for (size_t face = 0; face < axes.size(); ++face) {
            glm::vec4 value;
            const auto index = face * size * size + (size / 2) * size + size / 2;
            std::memcpy(&value, snapshot.pixels.data() + index * sizeof(value), sizeof(value));
            const auto expected = irradiance ? (glm::vec3{0.5f} + axes[face] / 3.0f) * std::numbers::pi_v<float> : glm::vec3{0.5f} + axes[face] * 0.5f;
            for (int c = 0; c < 3; ++c)
                VKZ_ASSERT(std::isfinite(value[c]) && std::abs(value[c] - expected[c]) < 0.035f, "Cubemap orientation and directional irradiance must match world axes");
        }
    }

}

int main() try {
    vkz::iostream_adapter::install(std::cout);
    vkz::vulkan_app app{{.width = 32, .height = 32, .title = "IBL regression", .validation = true}};
    glfwHideWindow(app.window());
    auto allocator = vkz::vma_memory_allocator::create(app.context());
    const auto queue = app.graphics_queue();
    const auto family = app.queue_family_index();
    temporary_hdr source;
    const glm::vec3 radiance{2, 4, 0.5f};
    std::vector<glm::vec3> pixels(32, radiance);
    VKZ_ASSERT(stbi_write_hdr(source.path.string().c_str(), 8, 4, 3, &pixels[0].x), "Write HDR fixture");
    auto maps = vkz::ibl::load(allocator, queue, family, source.path, {16, 8, 16});
    VKZ_ASSERT(maps.specular.image.create_info.mipLevels == 5, "Specular map must include the complete roughness chain");
    check_constant(vkz::ktx::read(allocator, queue, family, maps.environment), radiance);
    check_constant(vkz::ktx::read(allocator, queue, family, maps.irradiance), radiance * std::numbers::pi_v<float>);
    check_constant(vkz::ktx::read(allocator, queue, family, maps.specular), radiance);
    VKZ_ASSERT(maps.brdf_lut.image.create_info.format == VK_FORMAT_R8G8B8A8_UNORM, "BRDF LUT must be sampled in linear space");
    maps.destroy();
    pixels.resize(128 * 64);
    for (uint32_t y = 0; y < 64; ++y) {
        for (uint32_t x = 0; x < 128; ++x) {
            const float longitude = ((float(x) + 0.5f) / 128 - 0.5f) * 2 * std::numbers::pi_v<float>;
            const float latitude = (float(y) + 0.5f) / 64 * std::numbers::pi_v<float>;
            const glm::vec3 direction{std::sin(latitude) * std::cos(longitude), std::cos(latitude), std::sin(latitude) * std::sin(longitude)};
            pixels[y * 128 + x] = glm::vec3{0.5f} + direction * 0.5f;
        }
    }
    VKZ_ASSERT(stbi_write_hdr(source.path.string().c_str(), 128, 64, 3, &pixels[0].x), "Write directional HDR fixture");
    maps = vkz::ibl::load(allocator, queue, family, source.path, {33, 9, 17});
    check_axes(vkz::ktx::read(allocator, queue, family, maps.environment), false);
    check_axes(vkz::ktx::read(allocator, queue, family, maps.irradiance), true);
    maps.destroy();
    allocator.destroy();
    std::cout << "IBL HDR upload, cubemap conversion, irradiance energy and all specular mips passed\n";
    return 0;
} catch (const std::exception &error) {

    std::cerr << error.what() << '\n';
    return 1;
}
