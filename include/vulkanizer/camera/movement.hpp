#pragma once

#include "camera.hpp"

namespace vkz::camera {

    enum class movement_type { spectator, first_person, orbit };
    
    template<typename Scalar>
    class movement_t {
    protected:
        using Mat4 = glm::mat<4, 4, Scalar, glm::defaultp>;
        using Vec3 = glm::vec<3, Scalar, glm::defaultp>;
        using Vec2 = glm::vec<2, Scalar, glm::defaultp>;
        using Quat = glm::qua<Scalar, glm::defaultp>;
        using camera = camera_t<Scalar>;
    public:
        friend class input;
        friend class character_input;
        explicit movement_t(camera& camera);

        virtual ~movement_t() = default;

        virtual void look_at(const Vec3 &eye, const Vec3 &target, const Vec3 &up);

        virtual void rotate_smoothly(Scalar headingDegrees, Scalar pitchDegrees, Scalar rollDegrees);

        virtual void rotate(Scalar headingDegrees, Scalar pitchDegrees, Scalar rollDegrees) = 0;

        virtual void move(Scalar dx, Scalar dy, Scalar dz);

        virtual void move(const Vec3 &direction, const Vec3 &amount);
        
        void update_position(const Vec3& newPosition);
        
        virtual void position_changed() {};

        void update_position(const Vec3& direction, Scalar dt);

        void update_velocity(const Vec3& direction, Scalar dt);

        virtual void update(Scalar dt, Vec2 rotation_delta, Vec3 position_delta) = 0;

        [[nodiscard]]
        const camera& get_camera() const {
            return camera_;
        }

        virtual void undo_roll();

        virtual void zoom(Scalar amount);

        void perspective(Scalar fovx, Scalar aspect, Scalar znear, Scalar zfar);

        void perspective(Scalar aspect);

        [[nodiscard]] virtual bool handle_zoom() const;

    protected:
        virtual void update_view_matrix();

    protected:
        camera& camera_;
    };

    template<typename Scalar>
    class spectator_t : public movement_t<Scalar> {
        using Base = movement_t<Scalar>;
    public:
        explicit spectator_t(Base::camera& camera);

        void rotate(Scalar headingDegrees, Scalar pitchDegrees, Scalar rollDegrees) override;

        void update(Scalar dt, Base::Vec2 rotation_delta, Base::Vec3 position_delta) override;

    };

    template<typename Scalar>
    class first_person_t : public spectator_t<Scalar> {
        using Base = spectator_t<Scalar>;
    public:
        explicit first_person_t(camera_t<Scalar>& camera);

        void move(Scalar dx, Scalar dy, Scalar dz) override;
    };

    using movement = movement_t<float>;
    using dmovement = movement_t<double>;

    using spectator = spectator_t<float>;
    using dspectator = spectator_t<double>;

    using first_person = first_person_t<float>;
    using dfirst_person = first_person_t<double>;

}
