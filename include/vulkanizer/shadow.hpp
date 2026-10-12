#pragma once

#include "bvh.hpp"
#include "texture.hpp"
#include <memory>
#include <span>

namespace vkz {
    class shadow {
    public:
        struct scene_input {
            VkAccelerationStructureKHR tlas{};
            VkQueue queue{};
            uint32_t queue_family{};
            // Supply these borrowed inputs for alpha-tested/blended occluders.
            const mesh::data *scene{};
            mesh::geometry_buffers geometry{};
            std::span<const texture> material_textures;
        };

        struct frame_info {
            glm::mat4 inverse_view_projection{1};
            glm::mat4 view_projection{1};
            glm::vec3 camera_position{};
            bool denoise{true};
        };

        // Inputs are borrowed and must outlive this object. Keep the light span stable.
        // Depth uses Vulkan [0, 1]; normals use world-space octahedral XY encoding.
        // Requires ray queries, scalar block layout and storage image extended formats.
        shadow(vkz::device device, vma_memory_allocator &allocator,
               std::span<const mesh::light> lights, const texture &depth_buffer,
               const texture &normal_buffer, glm::uvec2 screen_resolution, bool half_resolution = true);

        ~shadow();

        shadow(const shadow &) = delete;
        shadow &operator=(const shadow &) = delete;

        void init(const scene_input &scene);
        void new_frame(const frame_info &frame);
        // Record after G-buffer inputs are SHADER_READ_ONLY_OPTIMAL, on the init queue.
        // new_frame -> exec -> end_frame; output remains GENERAL and shader-readable.
        void exec(VkCommandBuffer command_buffer);
        void end_frame();

        [[nodiscard]] const texture &visibility_texture() const;
        [[nodiscard]] const texture &penumbra_texture() const;

    private:
        void create_buffers();
        void init_textures();
        void create_descriptor_set_layouts();
        void create_descriptor_pool();
        void update_descriptors();
        void create_pipelines();
        void initialize_resources();

        struct implementation;
        std::unique_ptr<implementation> state_;
        vkz::device device_;
        vma_memory_allocator &allocator_;
        std::span<const mesh::light> lights_;
        const texture &depth_buffer_;
        const texture &normal_buffer_;
        glm::uvec2 screen_resolution_;
        bool half_resolution_;
    };
}
