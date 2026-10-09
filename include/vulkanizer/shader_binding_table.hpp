#pragma once

#include "memory.hpp"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vkz {
    enum class shader_group_type {
        ray_generation,
        miss,
        triangles_hit,
        procedural_hit,
        callable
    };

    struct shader_group_record {
        shader_group_type type{};
        uint32 group_index{};
        std::string name;
        std::vector<uint8_t> data;
    };

    class shader_binding_tables {
    public:
        shader_binding_tables() = default;

        ~shader_binding_tables();

        shader_binding_tables(const shader_binding_tables &) = delete;

        shader_binding_tables &operator=(const shader_binding_tables &) = delete;

        shader_binding_tables(shader_binding_tables &&other) noexcept;

        shader_binding_tables &operator=(shader_binding_tables &&other) noexcept;

        static shader_binding_tables
        create(vma_memory_allocator &allocator, VkPipeline pipeline, std::span<const shader_group_record> records);

        [[nodiscard]] const VkStridedDeviceAddressRegionKHR &ray_generation(std::string_view name) const;

        [[nodiscard]] uint32 hit_index(std::string_view name) const;

        [[nodiscard]] uint32 miss_index(std::string_view name) const;

        [[nodiscard]] uint32 callable_index(std::string_view name) const;

        void upload(VkCommandBuffer command_buffer) const;

        void destroy();

        VkStridedDeviceAddressRegionKHR miss{};
        VkStridedDeviceAddressRegionKHR hit{};
        VkStridedDeviceAddressRegionKHR callable{};

    private:
        struct named_region {
            std::string name;
            VkStridedDeviceAddressRegionKHR region{};
        };

        static uint32 index(std::span<const std::string> names, std::string_view name);

        void move_from(shader_binding_tables &&other);

        buffer storage;
        buffer staging;
        mutable bool upload_pending{};
        std::vector<named_region> ray_generations;
        std::vector<std::string> hit_names;
        std::vector<std::string> miss_names;
        std::vector<std::string> callable_names;
    };

}
