# Ray-tracing pipelines and shader binding tables

`vkz::ray_tracing_pipeline` derives from `vkz::pipeline`, so existing pipeline-binding helpers accept it. It adds `shader_binding_tables sbt` and owns its pipeline, layout, and SBT storage. It is move-only, releases these resources on destruction, and supports an explicit, repeatable `destroy()`. Keep the device and allocator alive and complete GPU work before destroying or replacing it. Pass it to base-pipeline APIs by reference; do not slice it or destroy it through the base type.

Include `<vulkanizer/ray_tracing_pipeline_builder.hpp>`, or use `make_ray_tracing_pipeline_builder(allocator)` from `<vulkanizer/builders.hpp>`.

```cpp
vkz::ray_tracing_pipeline_builder builder{allocator};

auto pipeline =
    builder
        .shader_groups()
            .ray_generation_group("primary")
                .ray_generation_shader(ray_generation_spirv)
            .miss_group("background")
                .miss_shader(miss_spirv)
            .triangles_hit_group("surface")
                .closest_hit_shader(closest_hit_spirv)
                .record_data(surface_record)
        .layout()
            .add_descriptor_set_layouts(descriptor_layouts)
            .add_push_constant_range(VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(constants_))
        .max_recursion_depth(1)
        .name("reference_ray_tracing")
        .build();
```

`shader_groups()` exposes ray-generation, miss, triangle-hit, procedural-hit, and callable groups. Each group has a unique name. Starting another group preserves the preceding group without an explicit `add()`. `hit_group(name)` selects an existing hit group for conditional mutation. Shader inputs use the same file paths, byte strings, and SPIR-V word vectors as existing builders. `entry_point(name)` and `add_specialization(value, constant_id)` configure the most recently selected shader. A procedural-hit group requires an intersection shader; triangle groups allow closest-hit and/or any-hit shaders.

`record_data(value)` copies one trivially copyable payload, replacing any previous payload for that group. A byte-span overload accepts explicitly packed records. The caller must match the shader-record block layout, including member padding. There is no automatic C++/GLSL layout conversion. Each group currently creates one SBT record; groups may use the same shader source with different payloads.

The builder creates the layout with the existing pipeline-layout utility, creates shader modules with `create_shader_module`, builds the Vulkan pipeline, queries packed handles, and creates an aligned, host-visible SBT buffer. It flushes host writes before returning, so no upload command buffer or hidden queue wait is needed. Enable `bufferDeviceAddress` and `rayTracingPipeline`, the required Vulkan extensions and dependencies, and create the allocator with `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT`.

`pipeline.sbt.ray_generation(name)` returns one ray-generation region. `miss`, `hit`, and `callable` are the remaining `VkStridedDeviceAddressRegionKHR` values, zero when unused. `hit_index`, `miss_index`, and `callable_index` return record indices within their respective regions. Region order follows declaration order within each category, independent of how categories are interleaved in the DSL. These indices are distinct from shader stage and pipeline group indices.

```cpp
vkz::bind_pipeline(command_buffer, pipeline);
vkz::trace_ray(command_buffer, pipeline, width, height);
```

Include `<vulkanizer/commands.hpp>` for `trace_ray`. It dispatches with depth 1 and defaults to the `"primary"` ray-generation record; pass another record name as the final argument to select it. The pipeline is passed by const reference. Clients still bind the pipeline, descriptors and push constants_. A BVH instance's `shader_binding_table_offset` can be set to `pipeline.sbt.hit_index("surface")`. The full hit-record index is `instance_offset + geometry_index * ray_stride + ray_offset`.

## Shader-binding-table demo

For the client-facing single-hit glTF pipeline, see the [mesh reference integration guide](reference_ray_tracing.md).

`vulkanizer_shader_binding_table_test` ports the vglib example to our builders, imported mesh format, and BVH. It converts the bundled Stanford Bunny GLB into the active build directory, displays three instances, and exposes hit-record colours, per-instance offsets, ray offset/stride, miss selection, instance cull masks, and mouse-driven orbit navigation. Hold the left mouse button to orbit and use the wheel to zoom; camera input is paused while any ImGui item is active. Each mesh BLAS currently contains one geometry, so its geometry index is zero; the UI states this limitation.

Run without arguments for the interactive example, with `--timeout 5` for a bounded UI run, or with `--verify` for GPU readback checks. The CTest entry uses `--verify`. The executable requires a Vulkan ray-tracing-capable GPU. Asset source and licence are in `test/mesh/resources/bunny`.

SBT layout follows the [Vulkan group-handle rules](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetRayTracingShaderGroupHandlesKHR.html) and [trace-ray region requirements](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdTraceRaysKHR.html).
