#pragma once

#include "texture.hpp"
#include "mesh.hpp"
#include <cstdint>
#include <span>
#include <memory>
#include "bvh.hpp"

namespace vkz {
    struct probes {
        glm::ivec3 count{20, 12, 20};
        glm::vec3 spacing{1};
        glm::vec3 inverse_spacing{1};
        uint32_t rays_per_probe{128};
        int32_t update_offsets{0};
        int32_t update_counts{0};
        float sphere_scale{0.1f};
        float max_offset{0.4f};
    };

    class ddgi {
    public:
        // Borrowed LOD-0 geometry and TLAS must use the same mesh instance order.
        // Vertex/index buffers need STORAGE_BUFFER usage. Their owner retains them.
        // The upload queue must support graphics, compute, transfer and ray tracing.
        struct scene_input {
            const mesh::data &scene;
            mesh::geometry_buffers geometry;
            VkAccelerationStructureKHR tlas{};
            VkQueue queue{};
            uint32_t queue_family{};
            // Optional borrowed material textures, including the white fallback at index 0.
            std::span<const texture> material_textures{};
        };

        struct frame_info {
            // Vulkan depth in [0, 1]; normals are world-space octahedral XY in [-1, 1].
            glm::mat4 inverse_view_projection{1};
            glm::vec3 camera_position{};
        };

        struct settings {
            vkz::probes probe_grid;
            glm::vec3 grid_position{-10, 0.5f, -10};
            uint32_t irradiance_side_length{6};
            uint32_t visibility_side_length{6};
            float hysteresis{0.95f};
            float self_shadow_bias{0.3f};
            float infinite_bounces_multiplier{0.75f};
            float max_ray_distance{100};
            bool half_resolution{};
            bool infinite_bounces{true};
            bool perceptual_encoding{true};
            bool relocate_probes{};
            bool classify_probes{};
        };

        // Requires ray tracing, acceleration structures, buffer device addresses,
        // scalar block layout, sampled-image array nonuniform indexing and storage
        // image extended formats. The VMA allocator needs buffer device addresses.
        // All constructor inputs are borrowed and must outlive this object. Keep the
        // light span's storage stable; light values may change between frames.
        ddgi(vkz::device device, vma_memory_allocator &allocator,
             std::span<const mesh::light> lights, const texture &depth_buffer,
             const texture &normal_buffer, glm::uvec2 screen_resolution);

        ~ddgi();

        ddgi(const ddgi &) = delete;

        ddgi &operator=(const ddgi &) = delete;

        // init(scene) creates DDGI-owned descriptors/pipelines and uploads resources.
        // configure must precede init. init() retries the previously supplied scene.
        void init();

        void init(const scene_input &scene);

        void configure(const settings &value);

        void new_frame();

        void new_frame(const frame_info &frame);

        // Record after depth and normal images are ready in SHADER_READ_ONLY_OPTIMAL.
        // Frame order: new_frame(frame), exec(cmd), end_frame(). Submit to the same
        // queue used at init; DDGI history is shared across frames and queue ordered.
        // Output stays in GENERAL, with shader writes visible to subsequent reads.
        void exec(VkCommandBuffer command_buffer);

        void probe_rt(VkCommandBuffer command_buffer);

        void update_irradiance(VkCommandBuffer command_buffer);

        void update_visibility(VkCommandBuffer command_buffer);

        void sample_indirect(VkCommandBuffer command_buffer);

        void end_frame();

        struct sampling_input {
            const buffer &constants;
            const buffer &status;
            const texture &irradiance;
            const texture &visibility;
            const texture &offset;
        };

        [[nodiscard]] sampling_input sampling_resources() const;

        [[nodiscard]] const texture &indirect_light_texture() const { return indirect_light; }

        [[nodiscard]] const texture &irradiance_atlas() const { return probe_grid_irradiance; }

        [[nodiscard]] const texture &visibility_atlas() const { return probe_grid_visibility; }

        [[nodiscard]] const vkz::probes &probe_grid() const { return probes_; }

    private:
        void create_buffers();

        void init_textures();

        void create_descriptor_pool();

        void create_descriptor_set_layouts();

        void update_descriptors();

        void create_probe_rt_pipeline();

        void init_compute_pipelines();

        void initialize_resources();

        void update_probes(VkCommandBuffer command_buffer);

        void destroy_resources();

        struct implementation;
        std::unique_ptr<implementation> state_;

        vkz::device device_;
        vma_memory_allocator &allocator_;
        std::span<const mesh::light> lights_;
        const texture &depth_buffer_;
        const texture &normal_buffer_;
        glm::uvec2 screen_resolution_;
        probes probes_;
        bool half_resolution_{};
        uint32_t irradiance_side_length_{6};
        uint32_t visibility_side_length_{6};

        uint32_t irradiance_atlas_width{};
        uint32_t irradiance_atlas_height{};
        uint32_t visibility_atlas_width{};
        uint32_t visibility_atlas_height{};

        texture indirect_light;
        texture radiance;
        texture irradiance;
        texture probe_grid_irradiance;
        texture probe_grid_visibility;
        texture probe_offset;
        texture probe_hit_geometry;
    };
}
