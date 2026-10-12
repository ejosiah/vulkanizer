#pragma once

#include <vulkanizer/context.hpp>
#include <vulkanizer/camera/camera.hpp>
#include <vulkanizer/texture.hpp>
#include <memory>

namespace rtx {
    class nrd_denoiser {
    public:
        struct input {
            const vkz::texture &depth;
            const vkz::texture &normal;
            const vkz::texture &surface;
            const vkz::texture &diffuse;
            const vkz::texture &specular;
            const vkz::texture &penumbra;
            vkz::texture &diffuse_output;
            vkz::texture &specular_output;
            vkz::texture &shadow_output;
        };

        nrd_denoiser(vkz::context &context, glm::uvec2 resolution, const input &inputs, int shadow_light = -1);
        ~nrd_denoiser();
        void exec(VkCommandBuffer cmd, const vkz::camera::camera &camera, const input &inputs, bool reset);

    private:
        struct implementation;
        std::unique_ptr<implementation> state_;
    };
}
