#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace vkz::camera {
    static constexpr double DEFAULT_FOVX = 90.0;
    static constexpr double DEFAULT_ZNEAR = 0.1;
    static constexpr double DEFAULT_ZFAR = 1000.0;
    static constexpr double HALF_PI = glm::half_pi<double>();
    static constexpr double PI = glm::pi<double>();
    static constexpr double TWO_PI = glm::two_pi<double>();

    static constexpr double DEFAULT_ROTATION_SPEED = 0.3f;
    static constexpr double DEFAULT_ZOOM_MAX = 5.0f;
    static constexpr double DEFAULT_ZOOM_MIN = 1.5f;

    static constexpr glm::vec3 DEFAULT_ACCELERATION(4.0f, 4.0f, 4.0f);
    static constexpr glm::vec3 DEFAULT_VELOCITY(1.0f);

    template<typename Scalar>
    inline constexpr glm::vec<3, Scalar, glm::defaultp> WORLD_XAXIS_T(Scalar(1), Scalar(0), Scalar(0));

    template<typename Scalar>
    inline constexpr glm::vec<3, Scalar, glm::defaultp> WORLD_YAXIS_T(Scalar(0), Scalar(1), Scalar(0));

    template<typename Scalar>
    inline constexpr glm::vec<3, Scalar, glm::defaultp> WORLD_ZAXIS_T(Scalar(0), Scalar(0), Scalar(1));

    constexpr glm::vec3 WORLD_XAXIS = WORLD_XAXIS_T<double>;
    constexpr glm::vec3 WORLD_YAXIS = WORLD_YAXIS_T<double>;
    constexpr glm::vec3 WORLD_ZAXIS = WORLD_ZAXIS_T<double>;

    template<typename Scalar>
    struct camera_t {
        using Mat4 = glm::mat<4, 4, Scalar, glm::defaultp>;
        using Vec3 = glm::vec<3, Scalar, glm::defaultp>;
        using Quat = glm::qua<Scalar, glm::defaultp>;

        Mat4 model{1};
        Mat4 view{1};
        Mat4 projection{1};
        Quat orientation{1, 0, 0, 0};
        Vec3 position{0};
        Vec3 xAxis{1, 0, 0};
        Vec3 yAxis{0, 1, 0};
        Vec3 zAxis{0, 0, 1};
        Vec3 target{0};
        Vec3 viewDir{0, 0, -1};
        Vec3 acceleration{DEFAULT_ACCELERATION};
        Vec3 velocity{DEFAULT_VELOCITY};
        Vec3 currentVelocity{0};
        Scalar fov{DEFAULT_FOVX};
        Scalar znear{DEFAULT_ZNEAR};
        Scalar zfar{DEFAULT_ZFAR};
        Scalar aspect_ratio{1};
        Scalar rotationSpeed_{DEFAULT_ROTATION_SPEED};
        Scalar accumPitchDegrees{};
        Scalar zoom{0};
        Scalar minZoom{DEFAULT_ZOOM_MIN};
        Scalar maxZoom{DEFAULT_ZOOM_MAX};
        Scalar rotationSpeed{DEFAULT_ROTATION_SPEED};
        Scalar yaw{0};
        Scalar pitch{0};
        Scalar roll{0};
        bool horizontal_fov{};
        bool moved{false};
    };

    using camera = camera_t<float>;
    using dcamera = camera_t<double>;
}
