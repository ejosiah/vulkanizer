#pragma once

#include "mesh.hpp"
#include "memory.hpp"
#include <memory>
#include <span>

namespace vkz::mesh {
    struct geometry_buffers {
        vkz::buffer vertices;
        vkz::buffer indices;
    };

    struct bvh_build_options {
        uint32_t lod{};
        bool allow_instance_updates{true};
    };

    struct bvh_instance_options {
        uint8_t mask{0xff};
        uint32_t shader_binding_table_offset{};
    };

    struct acceleration_structure_view {
        VkAccelerationStructureKHR handle{};
        VkDeviceAddress device_address{};
    };

    class bvh {
    public:
        explicit bvh(vma_memory_allocator &allocator);

        ~bvh();

        bvh(const bvh &) = delete;

        bvh &operator=(const bvh &) = delete;

        bvh(bvh &&) noexcept;

        bvh &operator=(bvh &&) noexcept;

        void build(VkCommandBuffer command_buffer, const data &scene, const geometry_buffers &geometry,
                   const bvh_build_options &options = {}, std::span<const bvh_instance_options> instances = {});

        void update_instances(VkCommandBuffer command_buffer, const data &scene,
                              std::span<const bvh_instance_options> instances = {});

        [[nodiscard]] acceleration_structure_view tlas() const;

        [[nodiscard]] acceleration_structure_view blas(uint32_t mesh_id) const;

        void destroy();

    private:
        struct impl;
        vma_memory_allocator *allocator{};
        std::unique_ptr<impl> state;
    };
}
