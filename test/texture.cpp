#define VKZ_IOSTREAM_ADAPTER

#include <vulkanizer/application.hpp>
#include <vulkanizer/imgui.hpp>
#include <vulkanizer/io.hpp>
#include <vulkanizer/texture.hpp>

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
    class texture_test final : public vkz::application {
    public:
        explicit texture_test(std::filesystem::path path)
            : application{"vulkanizer texture test", {1280, 800}}
            , path_{std::move(path)} {
        }

        ~texture_test() {
            vkz::imgui::remove_texture(descriptor_set_);
            texture_.destroy();
        }

        void setup() override {
            texture_ = vkz::load(
                allocator_,
                app_.graphics_queue(),
                app_.queue_family_index(),
                path_,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
            descriptor_set_ = vkz::imgui::add_texture(
                texture_.sampler,
                texture_.image_view,
                texture_.image.layout);
        }

        void app_logic(VkCommandBuffer) override {
            const auto display_size = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos({0.0f, 0.0f});
            ImGui::SetNextWindowSize(display_size);
            ImGui::Begin(
                "Texture",
                nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoSavedSettings);

            const auto available = ImGui::GetContentRegionAvail();
            const auto width = static_cast<float>(texture_.image.create_info.extent.width);
            const auto height = static_cast<float>(texture_.image.create_info.extent.height);
            const auto scale = std::min(available.x / width, available.y / height);
            const ImVec2 image_size{width * scale, height * scale};
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - image_size.x) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (available.y - image_size.y) * 0.5f);

            const auto texture_id = static_cast<ImTextureID>(
                reinterpret_cast<uintptr_t>(descriptor_set_));
            ImGui::Image(texture_id, image_size);
            ImGui::End();
        }

    private:
        std::filesystem::path path_;
        vkz::texture texture_{};
        VkDescriptorSet descriptor_set_{};
    };
}

int main(int argc, char** argv) {
    vkz::iostream_adapter::install(std::cout);

    if (argc != 2) {
        std::cerr << "Usage: vulkanizer_texture_test <image-path>\n";
        return 1;
    }

    const std::filesystem::path path{argv[1]};
    if (!std::filesystem::is_regular_file(path)) {
        std::cerr << "Image file does not exist: " << path << '\n';
        return 1;
    }

    try {
        texture_test app{path};
        app.main_loop();
    } catch (const std::exception& error) {
        std::cerr << "Texture test failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
