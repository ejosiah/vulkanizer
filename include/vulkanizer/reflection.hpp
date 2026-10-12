#pragma once

#include "ddgi.hpp"
#include "descriptors.hpp"
#include <array>
#include <memory>

namespace vkz {
    class reflection {
    public:
        struct scene_input {
            const mesh::data &scene;
            mesh::geometry_buffers geometry;
            VkAccelerationStructureKHR tlas{};
            VkQueue queue{};
            uint32_t queue_family{};
            std::span<const texture> material_textures;
        };

        struct frame_info {
            glm::mat4 inverse_view_projection{1};
            glm::mat4 view_projection{1};
            glm::vec3 camera_position{};
            bool enabled{true};
            bool denoise{true};
            bool use_ddgi{true};
        };

        // Inputs are borrowed. G-buffer surface is RGB F0 + perceptual roughness.
        // Mesh IDs are R32_UINT with zero for background. Output resolution is selected at construction.
        reflection(vkz::device device, vma_memory_allocator &allocator,
                   std::span<const mesh::light> lights, const texture &depth_buffer,
                   const texture &normal_buffer, const texture &surface_buffer,
                   const texture &mesh_id_buffer, glm::uvec2 screen_resolution, const ddgi &gi, bool half_resolution = true);

        ~reflection();
        reflection(const reflection &) = delete;
        reflection &operator=(const reflection &) = delete;

        void init(const scene_input &scene);
        void new_frame(const frame_info &frame);
        // Record after DDGI and sampled G-buffer inputs are ready, on the init queue.
        // new_frame -> exec -> end_frame; output remains GENERAL and shader-readable.
        void exec(VkCommandBuffer command_buffer);
        void end_frame();

        [[nodiscard]] const texture &reflection_texture() const;
        [[nodiscard]] const texture &raw_texture() const;

        struct tracing_input {
            std::array<descriptor_set_layout, 2> layouts;
            std::array<descriptor_set, 2> sets;
        };
        [[nodiscard]] tracing_input tracing_resources() const;

    private:
        void create_buffers();
        void init_textures();
        void create_descriptor_pool();
        void create_descriptor_set_layouts();
        void update_descriptors();
        void create_rt_pipeline();
        void create_compute_pipelines();
        void initialize_resources();

        struct implementation;
        std::unique_ptr<implementation> state_;
        vkz::device device_;
        vma_memory_allocator &allocator_;
        std::span<const mesh::light> lights_;
        const texture &depth_buffer_;
        const texture &normal_buffer_;
        const texture &surface_buffer_;
        const texture &mesh_id_buffer_;
        glm::uvec2 screen_resolution_;
        bool half_resolution_;
        const ddgi &gi_;
    };
}
