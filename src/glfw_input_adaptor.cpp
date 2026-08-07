#include "vulkanizer/glfw_input_adaptor.hpp"

#include <vulkanizer/log.hpp>

#include <algorithm>
#include <cmath>

namespace vkz {

    glfw_input_adaptor::glfw_input_adaptor(GLFWwindow *window) : window_{window} {}

       void glfw_input_adaptor::bind() {
           bind_keyboard_and_mouse();
           bind_game_pad();
       }

       void glfw_input_adaptor::bind_keyboard_and_mouse() {
           glfwSetWindowUserPointer(window_, this);
           glfwSetKeyCallback(window_, onKeyPress);
           glfwSetMouseButtonCallback(window_, onMouseClick);
           glfwSetCursorPosCallback(window_, onMouseMove);
           glfwSetScrollCallback(window_, onMouseWheelMove);
       }

       void glfw_input_adaptor::bind_game_pad() {

           for(auto id = 0; id < 1; ++id) {
               auto present = glfwJoystickIsGamepad(id);
               if (present == GLFW_TRUE) {
                   game_pad_ = id;
                   const auto name = glfwGetJoystickName(game_pad_.value());
                   vkz::info("found game pad: " + std::string{name});
               }
           }
       }

       void glfw_input_adaptor::process_game_pad_input() {
           if(!game_pad_.has_value()) return;

           GLFWgamepadstate state;

           if(glfwGetGamepadState(game_pad_.value(), &state)) {
               constexpr camera::mapping directions[]{
                   camera::mapping::forward, camera::mapping::back,
                   camera::mapping::left, camera::mapping::right,
                   camera::mapping::up, camera::mapping::down,
               };
               for (const auto direction : directions) {
                   game_pad_input_.mappings[direction] = {};
               }

               const auto activate = [this](camera::mapping direction, float speed) {
                   if (speed <= 0.0f) return;
                   auto& button = game_pad_input_.mappings[direction];
                   button.speed = button.held ? std::max(button.speed, speed) : speed;
                   button.held = true;
               };

               if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS) activate(camera::mapping::forward, 1.0f);
               if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS) activate(camera::mapping::back, 1.0f);
               if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_PRESS) activate(camera::mapping::left, 1.0f);
               if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS) activate(camera::mapping::right, 1.0f);

               const auto x = apply_dead_zone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
               const auto y = apply_dead_zone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
               activate(x < 0 ? camera::mapping::left : camera::mapping::right, std::abs(x));
               activate(y < 0 ? camera::mapping::forward : camera::mapping::back, std::abs(y));

               const auto right_trigger = std::clamp((state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 0.5f, 0.0f, 1.0f);
               const auto left_trigger = std::clamp((state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0f) * 0.5f, 0.0f, 1.0f);
               activate(camera::mapping::up, right_trigger);
               activate(camera::mapping::down, left_trigger);

               const glm::vec2 look{
                   apply_dead_zone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]),
                   apply_dead_zone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]),
               };
               input_device_.mouse.relative_position -= look * 10.0f;
               merge_directional_input();
           }
       }

       void glfw_input_adaptor::merge_directional_input() {
           constexpr camera::mapping directions[]{
               camera::mapping::forward, camera::mapping::back,
               camera::mapping::left, camera::mapping::right,
               camera::mapping::up, camera::mapping::down,
           };

           for (const auto direction : directions) {
               const auto& keyboard = keyboard_input_.mappings[direction];
               const auto& game_pad = game_pad_input_.mappings[direction];
               auto& combined = input_device_.mappings[direction];
               const auto was_held = combined.held;

               combined.held = keyboard.held || game_pad.held;
               combined.pressed = combined.held && !was_held;
               combined.speed = std::max(
                   keyboard.held ? keyboard.speed : 0.0f,
                   game_pad.held ? game_pad.speed : 0.0f);
           }
       }

       float glfw_input_adaptor::apply_dead_zone(float value) {
           constexpr float deadzone = 0.15f;
           if (std::abs(value) < deadzone)
               return 0.0f;

           // Rescale the remaining range smoothly back to [0, 1].
           return std::copysign(
               (std::abs(value) - deadzone) / (1.0f - deadzone),
               value
           );
       }

       void glfw_input_adaptor::onKeyPress(GLFWwindow *window, int key, int scancode, int action, int mods) {
           auto self = static_cast<glfw_input_adaptor*>(glfwGetWindowUserPointer(window));
           auto& input_source = self->keyboard_input_;
           camera::button* button{};

           if(key == GLFW_KEY_W) {
               button = &input_source.mappings[camera::mapping::forward];
           }
           if(key == GLFW_KEY_S) {
               button = &input_source.mappings[camera::mapping::back];
           }
           if(key == GLFW_KEY_A) {
               button = &input_source.mappings[camera::mapping::left];
           }
           if(key == GLFW_KEY_D) {
               button = &input_source.mappings[camera::mapping::right];
           }
           if(key == GLFW_KEY_E) {
               button = &input_source.mappings[camera::mapping::up];
           }
           if(key == GLFW_KEY_Q) {
               button = &input_source.mappings[camera::mapping::down];
           }

           if (!button) {
               return;
           }

           if(action == GLFW_PRESS) {
               button->held = true;
               button->speed = 1.0f;
           } else if (action == GLFW_RELEASE) {
               button->held = false;
               button->speed = 0.0f;
           }
           self->merge_directional_input();
       }

       void glfw_input_adaptor::onMouseClick(GLFWwindow *window, int mbutton, int action, int mods) {
           auto self = static_cast<glfw_input_adaptor*>(glfwGetWindowUserPointer(window));
           auto& input_source = self->input_device_;
           camera::button* button{};

           if(mbutton == GLFW_MOUSE_BUTTON_LEFT) {
               button = &input_source.mappings[camera::mapping::mouse_left];
           }
           if(mbutton == GLFW_MOUSE_BUTTON_MIDDLE) {
               button = &input_source.mappings[camera::mapping::mouse_middle];
           }
           if(mbutton == GLFW_MOUSE_BUTTON_RIGHT) {
               button = &input_source.mappings[camera::mapping::mouse_right];
           }

           if (!button) {
               return;
           }

           if(action == GLFW_PRESS) {
               button->pressed = true;
               button->held = true;
           } else if(action == GLFW_REPEAT) {
               button->held = true;
               button->pressed = false;
           } else if (action == GLFW_RELEASE) {
               button->pressed = button->held = false;
           }

       }

       void glfw_input_adaptor::onMouseMove(GLFWwindow *window, double x, double y) {
           auto self = static_cast<glfw_input_adaptor*>(glfwGetWindowUserPointer(window));
           auto& input_source = self->input_device_;
           input_source.mouse.position.x = static_cast<float>(x);
           input_source.mouse.position.y = static_cast<float>(y);

           const auto left_mouse_button = input_source.mappings[camera::mapping::mouse_left];
           static glm::vec2 previous_position{};

           if(left_mouse_button.held) {
               input_source.mouse.relative_position += previous_position - input_source.mouse.position;
           }
           previous_position = input_source.mouse.position;
       }

       void glfw_input_adaptor::onMouseWheelMove(GLFWwindow *window, double xOffset, double yOffset) {
           auto self = static_cast<glfw_input_adaptor*>(glfwGetWindowUserPointer(window));
           auto& input_source = self->input_device_;
           input_source.mouse.scroll_offset.x = static_cast<float>(xOffset);
           input_source.mouse.scroll_offset.y = static_cast<float>(yOffset);
       }
}