#include "vulkanizer/camera/controller.hpp"

#include <stdexcept>

namespace vkz::camera {

    template<typename Scalar>
    controller_t<Scalar>::controller_t(camera& camera, movement_type movement_type, input_device& device)
        : camera_(camera), device_(&device) {
        switch (movement_type) {
            case movement_type::spectator:
                movement_ = std::make_unique<spectator_t<Scalar>>(camera_);
                break;
            case movement_type::first_person:
                movement_ = std::make_unique<first_person_t<Scalar>>(camera_);
                break;
            default:
                throw std::invalid_argument("unsupported camera movement type");
        }
    }

    template<typename Scalar>
    void controller_t<Scalar>::process_input() {
        process_movement_input();
        process_zoom_input();
    }

    template<typename Scalar>
    void controller_t<Scalar>::update(float dt) {
        movement_->update(dt, device_->mouse.relative_position, direction_);
        device_->mouse.relative_position = {};
        device_->mouse.scroll_offset = {};
        for (auto& [mapping, button] : device_->mappings) {
            button.pressed = false;
        }
    }

    template<typename Scalar>
    const button& controller_t<Scalar>::mapped_button(mapping value) const {
        static constexpr button released{};
        const auto entry = device_->mappings.find(value);
        return entry == device_->mappings.end() ? released : entry->second;
    }

    template<typename Scalar>
    void controller_t<Scalar>::process_movement_input() {
        direction_ = Vec3(0);

        const auto process_axis = [this](
            mapping positive,
            mapping negative,
            int axis) {
            const auto& positive_button = mapped_button(positive);
            const auto& negative_button = mapped_button(negative);

            if (positive_button.pressed || negative_button.pressed) {
                camera_.currentVelocity[axis] = Scalar(0);
            }
            if (positive_button.held || positive_button.pressed) {
                direction_[axis] += Scalar(1);
            }
            if (negative_button.held || negative_button.pressed) {
                direction_[axis] -= Scalar(1);
            }
        };

        process_axis(mapping::right, mapping::left, 0);
        process_axis(mapping::up, mapping::down, 1);
        process_axis(mapping::forward, mapping::back, 2);
    }

    template<typename Scalar>
    void controller_t<Scalar>::process_zoom_input() {
        if (!movement_->handle_zoom()) {
            return;
        }

        auto amount = Scalar(0);
        if(device_->mouse.scroll_offset.y != 0) {
            if (device_->mouse.scroll_offset.y > 0) {
                amount = -static_cast<Scalar>(zoom_delta);
            } else {
                amount = static_cast<Scalar>(zoom_delta);
            }
        }

        if (amount != Scalar(0)) {
            movement_->zoom(amount);
        }
    }

    input_adaptor::input_adaptor() {
        input_device_.mappings[mapping::forward] = button{};
        input_device_.mappings[mapping::back] = button{};
        input_device_.mappings[mapping::left] = button{};
        input_device_.mappings[mapping::right] = button{};
        input_device_.mappings[mapping::up] = button{};
        input_device_.mappings[mapping::down] = button{};
        input_device_.mappings[mapping::mouse_left] = button{};
        input_device_.mappings[mapping::mouse_middle] = button{};
        input_device_.mappings[mapping::mouse_right] = button{};
    }

    input_device& input_adaptor::get_device() noexcept {
        return input_device_;
    }

    const input_device& input_adaptor::get_device() const noexcept {
        return input_device_;
    }

    template class controller_t<float>;
    template class controller_t<double>;
}
