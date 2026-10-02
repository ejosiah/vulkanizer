#pragma once

#include "graphics_pipeline_builder.hpp"
#include "compute_pipeline_builder.hpp"
#include "ray_tracing_pipeline_builder.hpp"
#include "descriptor_set_builder.hpp"

namespace vkz {

    graphics_pipeline_builder make_graphics_pipeline_builder(vkz::device device);

    compute_pipeline_builder make_compute_pipeline_builder(vkz::device device);

    ray_tracing_pipeline_builder make_ray_tracing_pipeline_builder(vma_memory_allocator& allocator);

    descriptor_set_layout_builder make_descriptor_set_layout_builder(vkz::device device);
}