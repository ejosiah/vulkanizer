#pragma once

#include "movement.hpp"

#include <map>
#include <memory>

namespace vkz::camera {

    template<typename Scalar>
    class controller_t {
    public:
        using camera = camera_t<Scalar>;
        using movement_ptr = std::unique_ptr<movement_t<Scalar>>;

        controller_t(camera& camera, movement_type movement_type);

        virtual ~controller_t() = default;

        controller_t(const controller_t&) = delete;
        controller_t& operator=(const controller_t&) = delete;
        controller_t(controller_t&&) = delete;
        controller_t& operator=(controller_t&&) = delete;

        virtual  void update(float dt) {};

        virtual void processInput() = 0;

    protected:
        camera& camera_;
        movement_ptr movement_;
    };

    struct button {
        bool pressed{};
        bool held{};
    };

    enum class mapping {
        forward,
        back,
        left,
        right,
        up,
        down,
        mouse_left,
        mouse_middle,
        mouse_right
    };

    struct input_device {
        std::map<mapping, button> mappings;
        struct {
            glm::vec2 position{};
            glm::vec2 relative_position{};
            glm::vec2 scroll_offset{};
        } mouse;
    };

    template<typename Scalar>
    class joystick_controller_t final : public controller_t<Scalar> {
        using Base = controller_t<Scalar>;
        using Vec3 = glm::vec<3, Scalar, glm::defaultp>;
        using camera = typename Base::camera;

    public:
        joystick_controller_t(camera& camera, movement_type movement_type, input_device& joystick);

        void processInput() override;

        void update(float dt) override;

    private:
        void processMovementInput();
        void processZoomInput();

        [[nodiscard]] const button& mappedButton(mapping value) const;

        input_device* joystick_;
        Vec3 direction_{};
        float zoom_delta{0.1};
    };

    class input_adaptor {
    public:
        input_adaptor();

        [[nodiscard]] input_device& get_joystick() noexcept;
        [[nodiscard]] const input_device& get_joystick() const noexcept;

        virtual void bind() = 0;

    protected:
        input_device input_device_{};
    };

    using controller = controller_t<float>;
    using dcontroller = controller_t<double>;
    using joystick_controller = joystick_controller_t<float>;
    using djoystick_controller = joystick_controller_t<double>;
}
