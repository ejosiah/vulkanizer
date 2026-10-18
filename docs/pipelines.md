# Pipeline builders

Include `<vulkanizer/builders.hpp>` for the graphics, compute, ray-tracing and
descriptor-layout factories. Each pipeline builder also has its own public header.
The snippets below assume a valid `vkz::device device`, enabled device features,
and application shader inputs. See [resources](resources.md) for buffer/image
ownership and [other builders](builders.md) for context, swapchain and descriptors.

## Reading the builder DSL

Place each chained call on its own line. A section selector is indented one level
under the builder, and the calls that configure that section are indented another
level. Returning to a sibling selector returns to the section-selector indentation.
The indentation expresses Vulkan state ownership; proxy objects let the chain
switch sections without an `end()` call.

## Graphics

This example draws a triangle whose vertex positions are produced by the vertex
shader, so no vertex bindings are required. `vertex_spirv` and `fragment_spirv`
are `std::vector<uint32_t>` shader binaries. `color_format` must match the actual
rendering attachment; `width` and `height` are positive framebuffer dimensions.

```cpp
auto graphics =
    vkz::make_graphics_pipeline_builder(device)
        .shader_stage()
            .vertex_shader(vertex_spirv)
            .fragment_shader(fragment_spirv)
        .input_assembly_state()
            .triangles()
        .viewport_state()
            .viewport()
                .origin(0.0f, 0.0f)
                .dimension(width, height)
                .min_depth(0.0f)
                .max_depth(1.0f)
            .scissor()
                .offset(0, 0)
                .extent(width, height)
        .rasterization_state()
            .polygon_mode_fill()
            .cull_none()
            .front_face_counter_clockwise()
            .line_width(1.0f)
        .multisample_state()
            .rasterization_samples(vkz::sample_count_x1)
        .depth_stencil_state()
            .disable_depth_test()
            .disable_depth_write()
        .color_blend_state()
            .attachment()
                .disable_blend()
                .color_write_mask(vkz::color_component_rgba)
        .dynamic_render_pass()
            .add_color_attachment(color_format)
            .enable()
        .name("triangle")
        .build();
```

Enable dynamic rendering on the device before using `dynamic_render_pass()`.
Despite its name, this section supplies `VkPipelineRenderingCreateInfo`; it does
not create a `VkRenderPass` or begin rendering. The application supplies compatible
attachments and begins/ends rendering during command recording.

For a conventional render pass, omit the dynamic-rendering section and use
`.render_pass(render_pass)` and `.subpass(index)`. Attachment formats, samples,
depth/stencil state and blend-attachment count must match the render configuration.
Pipeline construction does not create images or descriptor sets.

### Vertex input

For a vertex buffer, configure the binding and attributes before the input
assembly section. Here `vertex` is an application-defined standard-layout type
with a three-float `position` member. Include `<cstddef>` for `offsetof`.
Here and in the layout example, `builder` is an existing graphics pipeline builder.

```cpp
builder
    .vertex_input_state()
        .add_vertex_binding_description(0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX)
        .add_vertex_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, position));
```

### Layouts and push constants

Create descriptor layouts first, then supply them in set-number order. `set_layout`
is a `vkz::descriptor_set_layout`; `push_constants` is the application payload type.
Offsets and sizes are bytes and must match the shader declaration and device limits.

```cpp
builder
    .layout()
        .add_descriptor_set_layout(set_layout)
        .add_push_constant_range(vkz::shader_stage_vertex | vkz::shader_stage_fragment, 0, sizeof(push_constants));
```

The same layout pattern is available on compute and ray-tracing builders. It
describes the interface; allocating sets, writing descriptors, binding sets and
pushing values remain separate command-recording operations.

### Graphics state sections

| Selector | Configuration |
|---|---|
| `shader_stage()` | Vertex, fragment, geometry, tessellation, task and mesh shaders; per-shader specialization |
| `vertex_input_state()` | Vertex binding strides/rates and attribute formats/offsets |
| `input_assembly_state()` | Primitive topology and primitive restart |
| `tessellation_state()` | Patch control-point count and domain origin |
| `viewport_state()` | Nested `viewport()` and `scissor()` entries |
| `rasterization_state()` | Polygon mode, culling, winding, depth clamp/bias, discard and line width |
| `multisample_state()` | Sample count, sample shading, mask and alpha coverage |
| `depth_stencil_state()` | Depth tests/writes/bounds and nested front/back stencil operations |
| `color_blend_state()` | Blend constants, logic operation and nested blend attachments |
| `dynamic_state()` | Declare state that commands will set at draw time |
| `layout()` | Descriptor-set layouts and push-constant ranges |
| `dynamic_render_pass()` | Dynamic-rendering view mask and color/depth/stencil formats |

`viewport().add()`, `scissor().add()` and blend `attachment().add()` append the
current entry when building lists. The final pending entry is included by the
build operation. Configure every entry before appending it; use `clear()` on the
relevant section when replacing a list rather than extending it.

For dynamic viewport/scissor, still establish the required counts in the viewport
section, declare `.dynamic_state().viewport().scissor()`, and set both values in
the command buffer before drawing. Other dynamic-state selectors may require
additional extensions/features. Selecting a state does not enable those features.

## Compute

`compute_spirv` is a `std::vector<uint32_t>` containing a compute shader. The
specialization below sets constant ID 0 to an application-chosen value; use an ID
and value type that match the shader.

```cpp
auto compute =
    vkz::make_compute_pipeline_builder(device)
        .shader_stage()
            .compute_shader(compute_spirv)
            .entry_point("main")
            .add_specialization(uint32_t(64), 0)
        .layout()
            .add_descriptor_set_layout(set_layout)
        .name("simulation")
        .build();
```

The compute shader selector, entry point and specialization belong to
`compute_shader_stage_builder`; layout calls belong to
`compute_pipeline_layout_builder`. Keep an entry-point string passed via
`entry_point(const char*)` alive through pipeline creation.

Graphics and compute shader sources accept SPIR-V word vectors, `vkz::byte_string`
binary data, or `std::string` file paths to shader binaries. Graphics shaders also
accept inline GLSL strings whose first non-whitespace text is `#version`; these
are compiled in process. The compute builder treats strings as file paths, so
compile compute GLSL first or use the library's shader utilities.
Graphics `shader_builder` supports `add_specialization(value, constant_id)` for the
currently selected shader; its public interface does not provide the compute
builder's `entry_point()` setter.

## Ray tracing

The ray-tracing builder takes an allocator because it also creates shader binding
table storage. `ray_generation_spirv`, `miss_spirv` and `closest_hit_spirv` are
SPIR-V word vectors. Enable ray-tracing pipeline and buffer-device-address
features/extensions and create the allocator with the corresponding VMA
buffer-device-address flag before constructing this pipeline.

```cpp
auto ray_tracing =
    vkz::make_ray_tracing_pipeline_builder(allocator)
        .shader_groups()
            .ray_generation_group("primary")
                .ray_generation_shader(ray_generation_spirv)
            .miss_group("background")
                .miss_shader(miss_spirv)
            .triangles_hit_group("surface")
                .closest_hit_shader(closest_hit_spirv)
        .layout()
            .add_descriptor_set_layout(set_layout)
        .max_recursion_depth(1)
        .name("ray_tracing")
        .build();
```

Named group selectors append/select groups without a separate `add()`.
`procedural_hit_group()` requires an intersection shader; `callable_group()`
configures a callable shader. Per-group `record_data()` copies a shader-record
payload. See [ray tracing and shader binding tables](ray_tracing.md) for record
layout, group indices, dispatch and the executable example.

## Results, reuse and lifetimes

Graphics and compute `build()` return `vkz::pipeline` values containing a pipeline,
layout, bind point and device. These values need explicit `destroy()` after GPU
work completes; they do not release handles on scope exit. They are copyable
handle wrappers, so choose one owner and do not destroy copies independently.
`pipeline::destroy()` does not clear its handles; call it once for a live result.

`build(VkPipelineLayout&)` returns a raw pipeline handle and exposes the layout
produced by the builder. `build_native()` requires an already supplied layout;
it is not a shortcut for implicit layout construction. For ordinary application
code, prefer `build()` and one clearly owned result.

Graphics `.layout(existing_pipeline)` borrows an existing layout. The borrowed
layout is not owned by the new result: coordinate pipeline/layout destruction
explicitly rather than destroying both wrappers as independent owners. Do not
destroy the existing layout while a pipeline creation still needs it.

Successful graphics and compute builds clear shader stages. Supply them again
before another build. Graphics `reuse()` additionally clears vertex input,
descriptor layouts and push-constant ranges while retaining other state;
`clone()` copies graphics builder configuration for independent mutation.
Avoid reusing builders across creations until layout ownership is accounted for.
`create_info()` exposes builder-backed pointers, may create a layout and is not
a detached, owning snapshot. Keep the builder and its pointer-backed inputs alive
through the Vulkan call. Keep sample-mask arrays and extension chains alive too.

`vkz::ray_tracing_pipeline` differs: it is move-only and releases its pipeline,
layout and SBT automatically. Its explicit `destroy()` is repeatable. Keep the
device/allocator alive and finish GPU work before either cleanup path.
