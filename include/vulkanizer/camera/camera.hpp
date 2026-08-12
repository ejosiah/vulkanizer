#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>

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
        glm::vec3 position{0};
        Vec3 xAxis{1, 0, 0};
        Vec3 yAxis{0, 1, 0};
        Vec3 zAxis{0, 0, 1};
        Vec3 eyes{0};
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

    enum PlaneType : int { LEFT_PLANE = 0, RIGHT_PLANE, BOTTOM_PLANE, TOP_PLANE, NEAR_PLANE, FAR_PLANE};

    template<typename Scalar>
    using ClipPlaneT = glm::vec<4, Scalar, glm::defaultp>;

    template<typename Scalar>
    struct frustum_t {
        using Vec3 = glm::vec<3, Scalar, glm::defaultp>;
        using Mat4 = glm::mat<4, 4, Scalar, glm::defaultp>;
        using ClipPlane = ClipPlaneT<Scalar>;

        std::array<ClipPlane, 6> cp;

        bool test(const Vec3& point) const {
            using namespace glm;
            const auto v = ClipPlane(point, Scalar(1));
            auto outside = Scalar(0);
            outside += step(dot(cp[LEFT_PLANE], v), Scalar(0)) + step(dot(cp[RIGHT_PLANE], v), Scalar(0));
            outside += step(dot(cp[BOTTOM_PLANE], v), Scalar(0)) + step(dot(cp[TOP_PLANE], v), Scalar(0));
            outside += step(dot(cp[NEAR_PLANE], v), Scalar(0)) + step(dot(cp[FAR_PLANE], v), Scalar(0));

            return outside == Scalar(0);
        }

        bool test(const Vec3& boxMin, const Vec3& boxMax) const {
            using Vec4 = glm::vec<4, Scalar, glm::defaultp>;
            using namespace glm;

            auto corners = std::array<Vec4, 8> {{
                Vec4(boxMin.x, boxMin.y, boxMin.z, Scalar(1)), Vec4(boxMax.x, boxMin.y, boxMin.z, Scalar(1)), Vec4(boxMin.x, boxMax.y, boxMin.z, Scalar(1)),
                Vec4(boxMax.x, boxMax.y, boxMin.z, Scalar(1)), Vec4(boxMin.x, boxMin.y, boxMax.z, Scalar(1)), Vec4(boxMax.x, boxMin.y, boxMax.z, Scalar(1)),
                Vec4(boxMin.x, boxMax.y, boxMax.z, Scalar(1)), Vec4(boxMax.x, boxMax.y, boxMax.z, Scalar(1))
            }};

            for (int i = 0; i < 6; ++i) {
                auto outside = Scalar(0);
                outside += step(dot(cp[i], corners[0]), Scalar(0));
                outside += step(dot(cp[i], corners[1]), Scalar(0));
                outside += step(dot(cp[i], corners[2]), Scalar(0));
                outside += step(dot(cp[i], corners[3]), Scalar(0));
                outside += step(dot(cp[i], corners[4]), Scalar(0));
                outside += step(dot(cp[i], corners[5]), Scalar(0));
                outside += step(dot(cp[i], corners[6]), Scalar(0));
                outside += step(dot(cp[i], corners[7]), Scalar(0));

                if (outside == Scalar(8)) return false;
            }

            return true;
        }

        bool test(const Vec3& boxCenter, Scalar scale) {
            using Vec4 = glm::vec<4, Scalar, glm::defaultp>;
            using namespace glm;
            static auto corners = std::array<Vec4, 8> {{
                Vec4(Scalar(-0.5), Scalar(-0.5), Scalar(-0.5), Scalar(0.5)), Vec4(Scalar(0.5), Scalar(-0.5), Scalar(-0.5), Scalar(0.5)),
                Vec4(Scalar(0.5), Scalar(-0.5), Scalar(0.5), Scalar(0.5)), Vec4(Scalar(-0.5), Scalar(-0.5), Scalar(0.5), Scalar(0.5)),
                Vec4(Scalar(-0.5), Scalar(0.5), Scalar(-0.5), Scalar(0.5)), Vec4(Scalar(0.5), Scalar(0.5), Scalar(-0.5), Scalar(0.5)),
                Vec4(Scalar(0.5), Scalar(0.5), Scalar(0.5), Scalar(0.5)), Vec4(Scalar(-0.5), Scalar(0.5), Scalar(0.5), Scalar(0.5)),
            }};

            const auto bc = Vec4(boxCenter, Scalar(0.5));
            const auto s = Vec4(scale, scale, scale, Scalar(1));
            for (int i = 0; i < 6; ++i) {
                auto outside = Scalar(0);
                outside += step(dot(cp[i], bc + corners[0] * s), Scalar(0));
                outside += step(dot(cp[i], bc + corners[1] * s), Scalar(0));
                outside += step(dot(cp[i], bc + corners[2] * s), Scalar(0));
                outside += step(dot(cp[i], bc + corners[3] * s), Scalar(0));
                outside += step(dot(cp[i], bc + corners[4] * s), Scalar(0));
                outside += step(dot(cp[i], bc + corners[5] * s), Scalar(0));
                outside += step(dot(cp[i], bc + corners[6] * s), Scalar(0));
                outside += step(dot(cp[i], bc + corners[7] * s), Scalar(0));

                if (outside == Scalar(8)) return false;
            }

            return true;
        }

        static void extractFrustum(frustum_t& frustum, Mat4 M) {
            const auto m1 = glm::row(M, 0);
            const auto m4 = glm::row(M, 3);

            frustum.cp[LEFT_PLANE].x = m4[0] + m1[0];
            frustum.cp[LEFT_PLANE].y = m4[1] + m1[1];
            frustum.cp[LEFT_PLANE].z = m4[2] + m1[2];
            frustum.cp[LEFT_PLANE].w = m4[3] + m1[3];

            frustum.cp[RIGHT_PLANE].x = m4[0] - m1[0];
            frustum.cp[RIGHT_PLANE].y = m4[1] - m1[1];
            frustum.cp[RIGHT_PLANE].z = m4[2] - m1[2];
            frustum.cp[RIGHT_PLANE].w = m4[3] - m1[3];

            const auto m2 = glm::row(M, 1);

            frustum.cp[BOTTOM_PLANE].x = m4[0] + m2[0];
            frustum.cp[BOTTOM_PLANE].y = m4[1] + m2[1];
            frustum.cp[BOTTOM_PLANE].z = m4[2] + m2[2];
            frustum.cp[BOTTOM_PLANE].w = m4[3] + m2[3];

            frustum.cp[TOP_PLANE].x = m4[0] - m2[0];
            frustum.cp[TOP_PLANE].y = m4[1] - m2[1];
            frustum.cp[TOP_PLANE].z = m4[2] - m2[2];
            frustum.cp[TOP_PLANE].w = m4[3] - m2[3];

            const auto m3 = glm::row(M, 2);

            frustum.cp[NEAR_PLANE].x = m3[0];
            frustum.cp[NEAR_PLANE].y = m3[1];
            frustum.cp[NEAR_PLANE].z = m3[2];
            frustum.cp[NEAR_PLANE].w = m3[3];

            frustum.cp[FAR_PLANE].x = m4[0] - m3[0];
            frustum.cp[FAR_PLANE].y = m4[1] - m3[1];
            frustum.cp[FAR_PLANE].z = m4[2] - m3[2];
            frustum.cp[FAR_PLANE].w = m4[3] - m3[3];

            for (auto& p : frustum.cp) {
                auto invLength = glm::inversesqrt(glm::dot(p.xyz(), p.xyz()));
                p *= invLength;
            }
        }
    };

    using camera = camera_t<float>;
    using dcamera = camera_t<double>;
}
