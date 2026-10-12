#pragma once

#include <vulkanizer/context.hpp>
#include <vulkanizer/descriptors.hpp>
#include <glm/glm.hpp>
#include <memory>

namespace rtx {
    class neural_radiance_cache {
      public:
        neural_radiance_cache(vkz::context &context, uint32_t width, uint32_t height);

        ~neural_radiance_cache();

        neural_radiance_cache(const neural_radiance_cache &) = delete;

        neural_radiance_cache &operator=(const neural_radiance_cache &) = delete;

        void configure(glm::vec4 bounds, uint32_t bounces, bool reset);

        void begin_frame(VkCommandBuffer command_buffer);

        void resolve(VkCommandBuffer command_buffer, vkz::image &output, vkz::image_view view);

        void end_frame(VkQueue queue);

        vkz::descriptor_set_layout layout() const;

        vkz::descriptor_set bindings() const;

        glm::uvec2 training_dimensions() const;

        bool ready() const;

      private:
        struct implementation;
        std::unique_ptr<implementation> state_;
    };
}
