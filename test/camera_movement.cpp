#include <vulkanizer/camera/controller.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    template<typename Scalar>
    void test_orbit() {
        using Vec3 = glm::vec<3, Scalar, glm::defaultp>;
        using Vec4 = glm::vec<4, Scalar, glm::defaultp>;
        const auto near = [](const auto& a, const auto& b) { return glm::length(a - b) < Scalar(1e-4); };

        vkz::camera::camera_t<Scalar> camera;
        vkz::camera::orbit_t<Scalar> orbit(camera);
        require(near(camera.position, Vec3(0, 0, Scalar(vkz::camera::DEFAULT_ORBIT_OFFSET_DISTANCE))),
            "Default orbit must start away from its target");
        const Vec3 target(1, 2, 3);
        orbit.look_at(target + Vec3(0, 0, 4), target, Vec3(0, 1, 0));
        orbit.perspective(Scalar(55), Scalar(1.6), Scalar(0.1), Scalar(60));
        const auto projection = camera.projection;
        camera.moved = false;
        orbit.rotate(Scalar(90), Scalar(0), Scalar(0));
        require(camera.moved && near(camera.position, target + Vec3(4, 0, 0)), "Yaw must orbit the target");
        require(near(camera.view * Vec4(target, 1), Vec4(0, 0, -4, 1)), "Target must remain centered in view");
        require(near(camera.view * Vec4(camera.position, 1), Vec4(0, 0, 0, 1)), "View must map eye to origin");
        orbit.rotate(Scalar(0), Scalar(35), Scalar(0));
        require(std::abs(glm::length(camera.position - target) - Scalar(4)) < Scalar(1e-4), "Pitch must preserve orbit radius");
        require(near(camera.viewDir, glm::normalize(target - camera.position)), "Camera must face its target");

        const auto position = camera.position;
        orbit.move(Scalar(1), Scalar(2), Scalar(3));
        orbit.move(Vec3(1), Vec3(2));
        orbit.update(Scalar(1), {}, Vec3(1));
        require(near(camera.position, position) && near(camera.target, target), "Translation must not move an orbit camera");

        orbit.zoom(Scalar(-100));
        require(std::abs(glm::length(camera.position - target) - camera.minZoom) < Scalar(1e-4), "Zoom must clamp to minimum distance");
        orbit.zoom(Scalar(100));
        require(std::abs(glm::length(camera.position - target) - camera.maxZoom) < Scalar(1e-4), "Zoom must clamp to maximum distance");
        for (int i = 0; i < 4; ++i) require(near(camera.projection[i], projection[i]), "Orbit zoom must preserve projection");

        const Vec3 relocated(8, 9, 10);
        orbit.update_position(relocated);
        require(near(camera.target, relocated), "Explicit position change must relocate the target");
        require(std::abs(glm::length(camera.position - relocated) - camera.maxZoom) < Scalar(1e-4), "Relocation must preserve orbit radius");

        orbit.look_at(target + Vec3(0, 0, 4), target, Vec3(0, 1, 0));
        orbit.rotate(Scalar(0), Scalar(0), Scalar(45));
        require(near(camera.yAxis, Vec3(0, 1, 0)), "Target-axis mode must ignore roll");
        camera.preferTargetYAxisOrbiting = false;
        orbit.update(Scalar(0.5), {}, Vec3(1, 0, 0));
        require(!near(camera.yAxis, Vec3(0, 1, 0)), "Free orbit must support roll input");
        require(near(camera.position, target + Vec3(0, 0, 4)), "Roll must preserve eye position");
        orbit.undo_roll();
        require(near(camera.yAxis, Vec3(0, 1, 0)) && near(camera.target, target), "Undo roll must preserve target");

        vkz::camera::input_device input;
        vkz::camera::controller_t<Scalar> controller(camera, vkz::camera::movement_type::orbit, input);
        input.mouse.scroll_offset.y = 1;
        input.mouse.relative_position = {10, 5};
        controller.process_input();
        controller.update(0.016f);
        require(std::abs(camera.orbitOffsetDistance - Scalar(3.9)) < Scalar(1e-4), "Controller wheel must zoom by distance");
        require(!near(camera.position, target + Vec3(0, 0, Scalar(3.9))), "Controller mouse must rotate the orbit");
        const auto updated = camera.position;
        controller.process_input();
        controller.update(0.016f);
        require(near(camera.position, updated), "Controller must consume mouse and wheel deltas once");

        camera.preferTargetYAxisOrbiting = true;
        orbit.look_at(target + Vec3(0, 4, 0), target, Vec3(0, 0, 1));
        orbit.rotate(Scalar(90), Scalar(0), Scalar(0));
        require(near(camera.position, target + Vec3(-4, 0, 0)), "Yaw must respect a custom target up axis");
        for (int i = 0; i < 1000; ++i) orbit.rotate(Scalar(0.2), Scalar(0.1), Scalar(0));
        require(std::abs(glm::length(camera.position - target) - Scalar(4)) < Scalar(1e-4), "Repeated rotations must preserve radius");
        require(near(camera.view * Vec4(target, 1), Vec4(0, 0, -4, 1)), "Repeated rotations must keep the target centered");
    }
}

int main() {
    test_orbit<float>();
    test_orbit<double>();
}
