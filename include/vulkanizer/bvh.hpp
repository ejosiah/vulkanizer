#pragma once

#include "mesh.hpp"
#include "memory.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <span>

namespace vkz {
    struct geometry_buffers {
        vkz::buffer vertices;
        vkz::buffer indices;
    };

    struct acceleration_structure_view {
        VkAccelerationStructureKHR handle{};
        VkDeviceAddress device_address{};
    };

    class bvh {
      public:
        // Scene implementations resolve their own mesh layout and material policy.
        // Returned buffers are borrowed and must outlive recorded GPU work.
        struct scene {
            struct bvh_build_options {
                uint32_t lod{};
                bool allow_instance_updates{true};
            };

            struct bvh_instance_options {
                uint8_t mask{0xff};
                uint32_t shader_binding_table_offset{};
            };

            // Indexed float3 triangles; indices are uint32 and relative to the
            // first vertex. Implementations validate CPU indices when available.
            struct geometry {
                buffer vertices;
                buffer indices;
                VkDeviceSize vertex_offset{}; // Byte offset to the first position.
                VkDeviceSize vertex_stride{};
                uint32_t vertex_count{};
                VkDeviceSize index_offset{}; // Byte offset to uint32 triangle indices.
                uint32_t primitive_count{};
                // Change this whenever the GPU vertex/index contents change.
                uint64_t content_revision{};
            };

            struct instance {
                uint32_t mesh_id{};
                glm::mat4 model{1.0f};
                bvh_instance_options options;
                VkGeometryInstanceFlagsKHR flags{};
            };

            bvh_build_options build_options;

            virtual ~scene() = default;

            // Optional validation of CPU geometry before a full build. This is
            // intentionally skipped for instance updates.
            virtual void validate_geometry() const {
            }

            // Geometry order defines BLAS IDs. Accessors must return consistent
            // data throughout a build/update call. Geometry changes require build().
            [[nodiscard]] virtual size_t geometry_count() const = 0;
            [[nodiscard]] virtual geometry geometry_at(size_t index) const = 0;
            [[nodiscard]] virtual size_t instance_count() const = 0;
            [[nodiscard]] virtual instance instance_at(size_t index) const = 0;

            // GPU-driven scenes may write packed VkAccelerationStructureInstanceKHR
            // records directly, including inactive slots with a zero AS reference.
            // Write all instance_count() records, using the supplied BLAS addresses.
            [[nodiscard]] virtual bool gpu_instances() const {
                return false;
            }

            virtual void record_instances(VkCommandBuffer, const buffer &, std::span<const acceleration_structure_view>) const {
            }
        };

        using bvh_build_options = scene::bvh_build_options;
        using bvh_instance_options = scene::bvh_instance_options;

        explicit bvh(vma_memory_allocator &allocator);

        ~bvh();

        bvh(const bvh &) = delete;

        bvh &operator=(const bvh &) = delete;

        bvh(bvh &&) noexcept;

        bvh &operator=(bvh &&) noexcept;

        void build(VkCommandBuffer command_buffer, const scene &scene);

        // Rebuild changed GPU geometry without replacing AS handles. Layouts,
        // primitive counts, and instance-to-geometry assignments must be unchanged.
        // Synchronize previous readers before recording this operation.
        void rebuild(VkCommandBuffer command_buffer, const scene &scene);

        void update_instances(VkCommandBuffer command_buffer, const scene &scene);

        void build(VkCommandBuffer command_buffer, const mesh::data &scene, const geometry_buffers &geometry, const bvh_build_options &options = {},
                   std::span<const bvh_instance_options> instances = {});

        void update_instances(VkCommandBuffer command_buffer, const mesh::data &scene, std::span<const bvh_instance_options> instances = {});

        [[nodiscard]] acceleration_structure_view tlas() const;

        [[nodiscard]] acceleration_structure_view blas(uint32_t mesh_id) const;

        // Waits for submitted work and releases old build generations and
        // upload snapshots. Submit or discard recordings that reference them
        // before calling; retired handles must not be used afterward.
        void release_completed_resources();

        // Waits for submitted GPU work. Unsubmitted recordings referencing
        // this BVH must be submitted or discarded before destruction.
        void destroy();

      private:
        struct impl;
        vma_memory_allocator *allocator{};
        std::unique_ptr<impl> state;
        std::unique_ptr<impl> retired;

        void retire_state() noexcept;
        VkResult wait_for_resources() const noexcept;
    };

    using bvh_build_options = bvh::scene::bvh_build_options;
    using bvh_instance_options = bvh::scene::bvh_instance_options;

    namespace mesh {
        // Compatibility names for existing callers.
        using bvh = vkz::bvh;
        using geometry_buffers = vkz::geometry_buffers;
        using acceleration_structure_view = vkz::acceleration_structure_view;
        using bvh_build_options = vkz::bvh_build_options;
        using bvh_instance_options = vkz::bvh_instance_options;

        // Non-owning bridge for existing mesh scenes. Referenced data and options
        // must remain alive while the adapter is being used.
        struct scene_adapter final : bvh::scene {
            scene_adapter(const data &source, const geometry_buffers &geometry, const bvh_build_options &options = {}, std::span<const bvh_instance_options> instances = {});

            [[nodiscard]] size_t geometry_count() const override;
            void validate_geometry() const override;
            [[nodiscard]] geometry geometry_at(size_t index) const override;
            [[nodiscard]] size_t instance_count() const override;
            [[nodiscard]] instance instance_at(size_t index) const override;

          private:
            const data &source;
            geometry_buffers buffers;
            std::span<const bvh_instance_options> settings;
        };
    } // namespace mesh
} // namespace vkz
