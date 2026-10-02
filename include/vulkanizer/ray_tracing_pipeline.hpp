#pragma once

#include "pipeline.hpp"
#include "shader_binding_table.hpp"

namespace vkz {
    struct ray_tracing_pipeline : pipeline {
        ray_tracing_pipeline();

        ~ray_tracing_pipeline();

        ray_tracing_pipeline(const ray_tracing_pipeline &) = delete;

        ray_tracing_pipeline &operator=(const ray_tracing_pipeline &) = delete;

        ray_tracing_pipeline(ray_tracing_pipeline &&other) noexcept;

        ray_tracing_pipeline &operator=(ray_tracing_pipeline &&other) noexcept;

        void destroy();

        shader_binding_tables sbt;
    };
}
