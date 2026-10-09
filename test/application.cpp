#define VKZ_IOSTREAM_ADAPTER
#include <vulkanizer/application.hpp>
#include <stdexcept>

namespace {
    class frame_test final : public vkz::application {
    public:
        explicit frame_test(uint32_t count, bool depth = true)
            : application(vkz::vulkan_app_create_info{.width = 320, .height = 240,
                          .title = "Application frames in flight", .depth_testing = depth, .validation = true}, count) {}

        void new_frame() override {
            if (frame_index() != rendered_ % frames_in_flight())
                throw std::runtime_error("Unexpected frame slot");
        }
        void app_logic(VkCommandBuffer) override {
            ImGui::Begin("Frame resources");
            ImGui::Text("Slot %u of %u", frame_index(), frames_in_flight());
            ImGui::End();
        }
        void end_frame() override {
            ++rendered_;
            if (rendered_ == 12) glfwSetWindowSize(app_.window(), 400, 300);
            if (rendered_ == 24) glfwSetWindowSize(app_.window(), 320, 240);
            if (rendered_ == 48) glfwSetWindowShouldClose(app_.window(), GLFW_TRUE);
        }
    private:
        uint32_t rendered_{};
    };
}

int main(int argc, char **argv) try {
    vkz::iostream_adapter::install(std::cout);
    if (argc < 2 || argc > 3) throw std::invalid_argument("Expected frames-in-flight count [--no-depth]");
    const auto count = static_cast<uint32_t>(std::stoul(argv[1]));
    if (count == 0) {
        try { frame_test invalid(0); }
        catch (const std::invalid_argument &) { return 0; }
        throw std::runtime_error("Zero frames in flight was accepted");
    }
    frame_test app(count, argc < 3);
    app.main_loop();
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
