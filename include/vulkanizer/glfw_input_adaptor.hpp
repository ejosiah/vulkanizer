#pragma once

#include "camera/controller.hpp"

#include <GLFW/glfw3.h>

#include <optional>

namespace vkz {

    struct glfw_input_adaptor : public camera::input_adaptor {

        explicit glfw_input_adaptor(GLFWwindow *window, bool game_pad_enabled = false);

        void bind() override;

        void process_game_pad_input();

    private:
        void bind_keyboard_and_mouse();

        void bind_game_pad();

        void merge_directional_input();

        static float apply_dead_zone(float value);

        static void onMouseClick(GLFWwindow *window, int button, int action, int mods);

        static void onMouseMove(GLFWwindow *window, double x, double y);

        static void onKeyPress(GLFWwindow *window, int key, int scancode, int action, int mods);

        static void onMouseWheelMove(GLFWwindow *window, double xOffset, double yOffset);

        GLFWwindow *window_{};
        std::optional<int> game_pad_;
        camera::input_device keyboard_input_{};
        camera::input_device game_pad_input_{};
        bool game_pad_enabled_{};
    };

}