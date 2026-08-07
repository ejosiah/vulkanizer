#include "vulkanizer/camera/movement.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <algorithm>
#include <vector>

namespace {
    template<typename Scalar>
    bool close_enough(Scalar x, Scalar y, Scalar epsilon = Scalar(1E-3)) {
        return std::abs(x - y) <= epsilon * (std::abs(x) + std::abs(y) + Scalar(1));
    }

    template<typename Scalar>
    glm::mat<4, 4, Scalar, glm::defaultp> perspective_vfov_vk(Scalar fovy, Scalar aspect, Scalar zNear, Scalar zFar) {
        auto result = glm::perspectiveRH_ZO(fovy, aspect, zNear, zFar);
        result[1][1] *= Scalar(-1);
        return result;
    }

    template<typename Scalar>
    glm::mat<4, 4, Scalar, glm::defaultp> perspective_hfov_vk(Scalar fovx, Scalar aspect, Scalar zNear, Scalar zFar) {
        const auto fovy = Scalar(2) * std::atan(std::tan(fovx * Scalar(0.5)) / aspect);
        return perspective_vfov_vk(fovy, aspect, zNear, zFar);
    }

    template<typename Scalar>
    glm::mat<4, 4, Scalar, glm::defaultp> perspective_matrix(Scalar fov, Scalar aspect, Scalar zNear, Scalar zFar, bool horizontalFov) {
        return horizontalFov ? perspective_hfov_vk(fov, aspect, zNear, zFar)
                             : perspective_vfov_vk(fov, aspect, zNear, zFar);
    }
}

namespace vkz::camera {

    template<typename Scalar>
    movement_t<Scalar>::movement_t(movement_t::camera &camera) : camera_(camera) {}
    
    template<typename Scalar>
    void movement_t<Scalar>::look_at(const Vec3 &eye, const Vec3 &target, const Vec3 &up) {
        camera_.eyes = eye;
        camera_.target = target;
    
        auto& view = camera_.view;
        view = glm::lookAt(eye, target, up);
        camera_.accumPitchDegrees = glm::degrees(std::asin(view[1][2]));
    
        camera_.xAxis = Vec3(row(view, 0));
        camera_.yAxis = Vec3(row(view, 1));
        camera_.zAxis = Vec3(row(view, 2));
    
        camera_.viewDir = -camera_.zAxis;
    
        camera_.accumPitchDegrees = glm::degrees(std::asin(view[1][2]));
    
        camera_.orientation = Quat(view);
        update_view_matrix();
    }
    
    template<typename Scalar>
    void movement_t<Scalar>::update_view_matrix() {
        auto& view = camera_.view;
        view = glm::mat4_cast(camera_.orientation);
    
        camera_.xAxis = Vec3(glm::row(view, 0));
        camera_.yAxis = Vec3(glm::row(view, 1));
        camera_.zAxis = Vec3(glm::row(view, 2));
        camera_.viewDir = -camera_.zAxis;
    
        camera_.view[3][0] = -dot(camera_.xAxis, camera_.eyes);
        camera_.view[3][1] = -dot(camera_.yAxis, camera_.eyes);
        camera_.view[3][2] = -dot(camera_.zAxis, camera_.eyes);
        camera_.moved = true;
    }
    
    template<typename Scalar>
    void movement_t<Scalar>::rotate_smoothly(Scalar headingDegrees, Scalar pitchDegrees, Scalar rollDegrees) {
        headingDegrees *= camera_.rotationSpeed;
        pitchDegrees *= camera_.rotationSpeed;
        rollDegrees *= camera_.rotationSpeed;
    
        this->rotate(headingDegrees, pitchDegrees, rollDegrees);
    }

    template<typename Scalar>
    void movement_t<Scalar>::update_velocity(const movement_t::Vec3 &direction, Scalar dt) {

        const auto acceleration = camera_.acceleration;
        const auto velocity = camera_.velocity;
        auto& currentVelocity = camera_.currentVelocity;

        if (direction.x != Scalar(0)) {
            currentVelocity.x += direction.x * acceleration.x * dt;

            if (currentVelocity.x > velocity.x)
                currentVelocity.x = velocity.x;
            else if (currentVelocity.x < -velocity.x)
                currentVelocity.x = -velocity.x;
        } else {
            if (currentVelocity.x > Scalar(0)) {
                if ((currentVelocity.x -= acceleration.x * dt) < Scalar(0))
                    currentVelocity.x = Scalar(0);
            } else {
                if ((currentVelocity.x += acceleration.x * dt) > Scalar(0))
                    currentVelocity.x = Scalar(0);
            }
        }

        if (direction.y != Scalar(0)) {
            currentVelocity.y += direction.y * acceleration.y * dt;

            if (currentVelocity.y > velocity.y)
                currentVelocity.y = velocity.y;
            else if (currentVelocity.y < -velocity.y)
                currentVelocity.y = -velocity.y;
        } else {
            if (currentVelocity.y > Scalar(0)) {
                if ((currentVelocity.y -= acceleration.y * dt) < Scalar(0))
                    currentVelocity.y = Scalar(0);
            } else {
                if ((currentVelocity.y += acceleration.y * dt) > Scalar(0))
                    currentVelocity.y = Scalar(0);
            }
        }

        if (direction.z != Scalar(0)) {
            currentVelocity.z += direction.z * acceleration.z * dt;

            if (currentVelocity.z > velocity.z)
                currentVelocity.z = velocity.z;
            else if (currentVelocity.z < -velocity.z)
                currentVelocity.z = -velocity.z;
        } else {
            if (currentVelocity.z > Scalar(0)) {
                if ((currentVelocity.z -= acceleration.z * dt) < Scalar(0))
                    currentVelocity.z = Scalar(0);
            } else {
                if ((currentVelocity.z += acceleration.z * dt) > Scalar(0))
                    currentVelocity.z = Scalar(0);
            }
        }
    }

    template<typename Scalar>
    void movement_t<Scalar>::update_position(const movement_t::Vec3 &direction, Scalar dt) {
        using namespace glm;
        const auto currentVelocity = camera_.currentVelocity;
        const auto acceleration = camera_.acceleration;

        if (dot(currentVelocity, currentVelocity) != Scalar(0)) {
            Vec3 displacement = (currentVelocity * dt) + (Scalar(0.5) * acceleration * dt * dt);

            if (direction.x == Scalar(0) && close_enough(currentVelocity.x, Scalar(0)))
                displacement.x = Scalar(0);

            if (direction.y == Scalar(0) && close_enough(currentVelocity.y, Scalar(0)))
                displacement.y = Scalar(0);

            if (direction.z == Scalar(0) && close_enough(currentVelocity.z, Scalar(0)))
                displacement.z = Scalar(0);

            move(displacement.x, displacement.y, displacement.z);
        }

        update_velocity(direction, dt);
    }

    template<typename Scalar>
    void movement_t<Scalar>::move(const movement_t::Vec3 &direction, const movement_t::Vec3 &amount) {
        camera_.eyes.x += direction.x * amount.x;
        camera_.eyes.y += direction.y * amount.y;
        camera_.eyes.z += direction.z * amount.z;

        update_view_matrix();
    }

    template<typename Scalar>
    void movement_t<Scalar>::update_position(const movement_t::Vec3 &newPosition) {
        camera_.eyes = newPosition;
        position_changed();
        update_view_matrix();
    }

    template<typename Scalar>
    void movement_t<Scalar>::move(Scalar dx, Scalar dy, Scalar dz) {
        if (dx == Scalar(0) && dy == Scalar(0) && dz == Scalar(0)) return;

        const Vec3 forwards = camera_.viewDir;
        Vec3 eyes = camera_.eyes;

        eyes += camera_.xAxis * dx;
        eyes += WORLD_YAXIS_T<Scalar> * dy;
        eyes += forwards * dz;

        update_position(eyes);
    }

    template<typename Scalar>
    void movement_t<Scalar>::zoom(Scalar amount) {
        camera_.zoom = std::clamp(camera_.zoom + amount, camera_.minZoom, camera_.maxZoom);
        perspective(camera_.zoom, camera_.aspect_ratio, camera_.znear, camera_.zfar);
    }

    template<typename Scalar>
    void movement_t<Scalar>::undo_roll() {
        look_at(camera_.eyes, camera_.eyes + camera_.viewDir, WORLD_YAXIS_T<Scalar>);
    }

    template<typename Scalar>
    void movement_t<Scalar>::perspective(Scalar aspect) {
        perspective(camera_.fov, aspect, camera_.znear, camera_.zfar);
    }

    template<typename Scalar>
    void movement_t<Scalar>::perspective(Scalar fov, Scalar aspect, Scalar znear, Scalar zfar) {
        camera_.projection = perspective_matrix(glm::radians(fov), aspect, znear, zfar, camera_.horizontal_fov);
        camera_.fov = fov;
        camera_.aspect_ratio = aspect;
        camera_.znear = znear;
        camera_.zfar = zfar;
        camera_.moved = true;
    }

    template<typename Scalar>
    bool movement_t<Scalar>::handle_zoom() const {
        return true;
    }

    template<typename Scalar>
    spectator_t<Scalar>::spectator_t(Base::camera &camera):Base(camera) {}


    template<typename Scalar>
    void spectator_t<Scalar>::rotate(Scalar headingDegrees, Scalar pitchDegrees, Scalar rollDegrees) {
        if (headingDegrees == Scalar(0) && pitchDegrees == Scalar(0) && rollDegrees == Scalar(0)) {
            return;
        }

        auto& camera = this->camera_;
        camera.accumPitchDegrees += pitchDegrees;

        if (camera.accumPitchDegrees > Scalar(90)) {
            pitchDegrees = Scalar(90) - (camera.accumPitchDegrees - pitchDegrees);
            camera.accumPitchDegrees = Scalar(90);
        }

        if (camera.accumPitchDegrees < Scalar(-90)) {
            pitchDegrees = Scalar(-90) - (camera.accumPitchDegrees - pitchDegrees);
            camera.accumPitchDegrees = Scalar(-90);
        }

        typename Base::Quat rot{};

        if (headingDegrees != Scalar(0)) {
            rot = glm::angleAxis(glm::radians(headingDegrees), WORLD_YAXIS_T<Scalar>);
            camera.orientation = camera.orientation * rot;
        }

        if (pitchDegrees != Scalar(0)) {
            rot = glm::angleAxis(glm::radians(pitchDegrees), WORLD_XAXIS_T<Scalar>);
            camera.orientation = rot * camera.orientation;
        }
        this->update_view_matrix();
    }

    template<typename Scalar>
    void spectator_t<Scalar>::update(Scalar dt, Base::Vec2 rotation_delta, Base::Vec3 position_delta) {
        const auto dx = -rotation_delta.x;
        const auto dy = -rotation_delta.y;
        this->rotate_smoothly(dx, dy, Scalar(0));
        this->update_position(position_delta, static_cast<Scalar>(dt));
    }
    
    template<typename Scalar>
    first_person_t<Scalar>::first_person_t(camera_t<Scalar>& camera):Base(camera) {}

    template<typename Scalar>
    void first_person_t<Scalar>::move(Scalar dx, Scalar dy, Scalar dz) {
        if (dx == Scalar(0) && dy == Scalar(0) && dz == Scalar(0)) return;
        auto& camera = this->camera_;
        auto eyes = camera.eyes;
    
        auto forwards = normalize(cross(WORLD_YAXIS_T<Scalar>, camera.xAxis));
    
        eyes += camera.xAxis * dx;
        eyes += WORLD_YAXIS_T<Scalar> * dy;
        eyes += forwards * dz;

        this->update_position(eyes);
    }

    template class spectator_t<float>;
    template class spectator_t<double>;
    template class first_person_t<float>;
    template class first_person_t<double>;
}

