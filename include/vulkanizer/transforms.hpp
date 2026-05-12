#pragma once

#define GLM_FORCE_RIGHT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vkz {

    constexpr glm::mat4 GL_TO_VKN_CLIP(1.0f,  0.0f, 0.0f, 0.0f,
                                      0.0f, -1.0f, 0.0f, 0.0f,
                                      0.0f,  0.0f, 0.5f, 0.0f,
                                      0.0f,  0.0f, 0.5f, 1.0f);

    glm::mat4 perspectiveHFov(float fovx, float aspect, float znear, float zfar);

    glm::mat4 perspectiveVFov(float fovy, float aspect, float near, float far);

    inline glm::mat4 ortho(float left, float right, float bottom, float top, float near, float far) {
        return GL_TO_VKN_CLIP * glm::ortho(left, right, bottom, top, near, far);
    }

    inline glm::mat4 ortho(float left, float right, float bottom, float top) {
        return GL_TO_VKN_CLIP * glm::ortho(left, right, bottom, top);
    }

//    inline glm::mat4 perspective(float fov, float aspect, float zNear, float zFar, bool horizontalFov = false){
//        auto projection = horizontalFov ? perspectiveHFov(fov, aspect, zNear, zFar) : perspectiveVFov(fov, aspect, zNear, zFar);
//        return  projection;
//    }

    inline glm::mat4 perspectiveVFov_VK(float fovy, float aspect, float zNear, float zFar) {
        glm::mat4 P = glm::perspectiveRH_ZO(fovy, aspect, zNear, zFar); // RH, depth 0..1
        P[1][1] *= -1.0f; // Vulkan framebuffer has inverted Y
        return P;
    }

    inline glm::mat4 perspectiveHFov_VK(float fovx, float aspect, float zNear, float zFar) {
        // fovx is horizontal FoV in radians; aspect = width/height
        float fovy = 2.0f * atanf(tanf(fovx * 0.5f) / aspect);
        return perspectiveVFov_VK(fovy, aspect, zNear, zFar);
    }

    inline glm::mat4 perspective(float fov, float aspect, float zNear, float zFar, bool horizontalFov=false) {
        return horizontalFov ? perspectiveHFov_VK(fov, aspect, zNear, zFar)
                             : perspectiveVFov_VK(fov, aspect, zNear, zFar);
    }

    inline glm::mat4 perspectiveHFov(float fovx, float aspect, float znear, float zfar){
        float e = 1.0f / tanf(fovx / 2.0f);
        float aspectInv = 1.0f / aspect;
        float fovy = 2.0f * atanf(aspectInv / e);
        float xScale = 1.0f / tanf(0.5f * fovy);
        float yScale = xScale / aspectInv;

        glm::mat4 Result{0};
        Result[0][0] = xScale;
        Result[1][1] = -yScale;
        Result[2][2] = zfar / (znear - zfar);
        Result[2][3] = -1.0f;
        Result[3][2] = (zfar * znear) / (znear - zfar);

        return Result;
    }

    inline glm::mat4 perspectiveVFov(float fovy, float aspect, float zNear, float zFar){
        assert(abs(aspect - std::numeric_limits<float>::epsilon()) > 0);

        float const tanHalfFovy = glm::tan(fovy / 2);

        glm::mat4 Result(0);
        Result[0][0] = 1 / (aspect * tanHalfFovy);
        Result[1][1] = -1 / (tanHalfFovy);
        Result[2][2] = -zFar / (zFar - zNear);
        Result[2][3] = -1;
        Result[3][2] = -(zFar * zNear) / (zFar - zNear);
        return Result;
    }

}