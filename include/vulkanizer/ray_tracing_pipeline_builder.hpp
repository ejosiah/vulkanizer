#pragma once

#include "ray_tracing_pipeline.hpp"
#include "compute_pipeline_builder.hpp"
#include "status.hpp"
#include <memory>
#include <type_traits>

namespace vkz {
    class ray_tracing_pipeline_builder;

    class ray_tracing_shader_group_builder;

    class ray_tracing_pipeline_layout_builder;

    template<typename Derived>
    class ray_tracing_pipeline_builder_proxy {
    public:
        explicit ray_tracing_pipeline_builder_proxy(ray_tracing_pipeline_builder &parent) : parent(parent) {
        }

        ray_tracing_pipeline_builder &shader_groups();

        ray_tracing_shader_group_builder &ray_generation_group(const std::string &name);

        ray_tracing_shader_group_builder &miss_group(const std::string &name);

        ray_tracing_shader_group_builder &triangles_hit_group(const std::string &name);

        ray_tracing_shader_group_builder &procedural_hit_group(const std::string &name);

        ray_tracing_shader_group_builder &callable_group(const std::string &name);

        ray_tracing_shader_group_builder &hit_group(std::string_view name);

        ray_tracing_pipeline_layout_builder &layout();

        Derived &max_recursion_depth(uint32_t value);

        Derived &name(const std::string &value);

        Derived &pipeline_cache(VkPipelineCache value);

        ray_tracing_pipeline build();

    protected:
        ray_tracing_pipeline_builder &parent;
    };

    class ray_tracing_shader_group_builder
            : public ray_tracing_pipeline_builder_proxy<ray_tracing_shader_group_builder> {
    public:
        using shader_source = compute_shader_stage_builder::shader_source;

        ray_tracing_shader_group_builder(ray_tracing_pipeline_builder &parent, shader_group_type type,
                                         std::string name);

        ray_tracing_shader_group_builder &ray_generation_shader(const shader_source &source);

        ray_tracing_shader_group_builder &miss_shader(const shader_source &source);

        ray_tracing_shader_group_builder &closest_hit_shader(const shader_source &source);

        ray_tracing_shader_group_builder &any_hit_shader(const shader_source &source);

        ray_tracing_shader_group_builder &intersection_shader(const shader_source &source);

        ray_tracing_shader_group_builder &callable_shader(const shader_source &source);

        ray_tracing_shader_group_builder &entry_point(std::string value);

        ray_tracing_shader_group_builder &record_data(std::span<const uint8_t> bytes);

        template<typename T>
        ray_tracing_shader_group_builder &record_data(const T &value) {
            static_assert(std::is_trivially_copyable_v<T>);
            return record_data(std::span<const uint8_t>{reinterpret_cast<const uint8_t *>(&value), sizeof(T)});
        }

        template<typename T>
        ray_tracing_shader_group_builder &add_specialization(T value, uint32_t constant_id) {
            static_assert(std::is_trivially_copyable_v<T>);
            return specialization(std::span<const uint8_t>{reinterpret_cast<const uint8_t *>(&value), sizeof(T)},
                                  constant_id);
        }

    private:
        friend class ray_tracing_pipeline_builder;

        struct stage {
            vkz::shader_stage type{};
            shader_source source;
            std::string entry{"main"};
            std::vector<VkSpecializationMapEntry> entries;
            std::vector<uint8_t> constants;
        };

        ray_tracing_shader_group_builder &add_shader(const shader_source &source, vkz::shader_stage type);

        ray_tracing_shader_group_builder &specialization(std::span<const uint8_t> bytes, uint32_t constant_id);

        shader_group_type type;
        std::string group_name;
        std::vector<stage> stages;
        std::vector<uint8_t> record;
    };

    class ray_tracing_pipeline_layout_builder
            : public ray_tracing_pipeline_builder_proxy<ray_tracing_pipeline_layout_builder> {
    public:
        explicit ray_tracing_pipeline_layout_builder(ray_tracing_pipeline_builder &parent);

        ray_tracing_pipeline_layout_builder &add_descriptor_set_layout(descriptor_set_layout value);

        template<typename Layouts>
        ray_tracing_pipeline_layout_builder &add_descriptor_set_layouts(const Layouts &values) {
            for (const auto &value: values)
                add_descriptor_set_layout(value);
            return *this;
        }

        ray_tracing_pipeline_layout_builder &add_push_constant_range(vkz::shader_stage stages, uint32_t offset, uint32_t size);

        ray_tracing_pipeline_layout_builder &add_push_constant_range(VkPushConstantRange value);

        ray_tracing_pipeline_layout_builder &clear();
    };

    class ray_tracing_pipeline_builder {
    public:
        explicit ray_tracing_pipeline_builder(vma_memory_allocator &allocator);

        ~ray_tracing_pipeline_builder();

        ray_tracing_pipeline_builder(const ray_tracing_pipeline_builder &) = delete;

        ray_tracing_pipeline_builder &operator=(const ray_tracing_pipeline_builder &) = delete;

        ray_tracing_pipeline_builder &shader_groups();

        ray_tracing_shader_group_builder &ray_generation_group(const std::string &name);

        ray_tracing_shader_group_builder &miss_group(const std::string &name);

        ray_tracing_shader_group_builder &triangles_hit_group(const std::string &name);

        ray_tracing_shader_group_builder &procedural_hit_group(const std::string &name);

        ray_tracing_shader_group_builder &callable_group(const std::string &name);

        ray_tracing_shader_group_builder &hit_group(std::string_view name);

        ray_tracing_pipeline_layout_builder &layout();

        ray_tracing_pipeline_builder &max_recursion_depth(uint32_t value);

        ray_tracing_pipeline_builder &name(const std::string &value);

        ray_tracing_pipeline_builder &pipeline_cache(VkPipelineCache value);

        ray_tracing_pipeline build();

    private:
        friend class ray_tracing_pipeline_layout_builder;

        ray_tracing_shader_group_builder &add_group(shader_group_type type, const std::string &name);

        void validate(const VkPhysicalDeviceRayTracingPipelinePropertiesKHR &properties) const;

        vma_memory_allocator &allocator;
        compute_pipeline_builder layout_storage;
        ray_tracing_pipeline_layout_builder layout_proxy;
        std::vector<std::unique_ptr<ray_tracing_shader_group_builder>> groups;
        uint32_t recursion_depth{1};
        std::string pipeline_name;
        VkPipelineCache cache{};
    };

    template<typename Derived>
    ray_tracing_pipeline_builder &ray_tracing_pipeline_builder_proxy<Derived>::shader_groups() {
        return parent.shader_groups();
    }

    template<typename Derived>
    ray_tracing_shader_group_builder &
    ray_tracing_pipeline_builder_proxy<Derived>::ray_generation_group(const std::string &name) {
        return parent.ray_generation_group(name);
    }

    template<typename Derived>
    ray_tracing_shader_group_builder &ray_tracing_pipeline_builder_proxy<Derived>::miss_group(const std::string &name) {
        return parent.miss_group(name);
    }

    template<typename Derived>
    ray_tracing_shader_group_builder &
    ray_tracing_pipeline_builder_proxy<Derived>::triangles_hit_group(const std::string &name) {
        return parent.triangles_hit_group(name);
    }

    template<typename Derived>
    ray_tracing_shader_group_builder &
    ray_tracing_pipeline_builder_proxy<Derived>::procedural_hit_group(const std::string &name) {
        return parent.procedural_hit_group(name);
    }

    template<typename Derived>
    ray_tracing_shader_group_builder &
    ray_tracing_pipeline_builder_proxy<Derived>::callable_group(const std::string &name) {
        return parent.callable_group(name);
    }

    template<typename Derived>
    ray_tracing_shader_group_builder &ray_tracing_pipeline_builder_proxy<Derived>::hit_group(std::string_view name) {
        return parent.hit_group(name);
    }

    template<typename Derived>
    ray_tracing_pipeline_layout_builder &ray_tracing_pipeline_builder_proxy<Derived>::layout() {
        return parent.layout();
    }

    template<typename Derived>
    Derived &ray_tracing_pipeline_builder_proxy<Derived>::max_recursion_depth(uint32_t value) {
        parent.max_recursion_depth(value);
        return static_cast<Derived &>(*this);
    }

    template<typename Derived>
    Derived &ray_tracing_pipeline_builder_proxy<Derived>::name(const std::string &value) {
        parent.name(value);
        return static_cast<Derived &>(*this);
    }

    template<typename Derived>
    Derived &ray_tracing_pipeline_builder_proxy<Derived>::pipeline_cache(VkPipelineCache value) {
        parent.pipeline_cache(value);
        return static_cast<Derived &>(*this);
    }

    template<typename Derived>
    ray_tracing_pipeline ray_tracing_pipeline_builder_proxy<Derived>::build() {
        return parent.build();
    }
}
