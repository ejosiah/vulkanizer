#pragma once
#include "texture.hpp"
#include <memory>

namespace vkz::ibl {
    struct settings {
        uint32_t environment_size{512};
        uint32_t irradiance_size{32};
        uint32_t specular_size{256};
    };

    struct maps {
        void destroy();

        texture environment;
        texture irradiance;
        texture specular;
        texture brdf_lut;
    };

    class generator {
      public:
        explicit generator(vkz::device device);
        ~generator();
        generator(const generator &) = delete;
        generator &operator=(const generator &) = delete;
        void convert(VkCommandBuffer command_buffer, const texture &equirectangular, texture &environment);
        void generate(VkCommandBuffer command_buffer, const texture &environment, texture &irradiance, texture &specular, VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

      private:
        struct implementation;
        std::unique_ptr<implementation> state;
    };

    [[nodiscard]] texture load_brdf_lut(vma_memory_allocator &allocator, VkQueue queue, uint32_t queue_family);
    [[nodiscard]] maps load(vma_memory_allocator &allocator, VkQueue queue, uint32_t queue_family, const std::filesystem::path &file, const settings &options = {});
}
