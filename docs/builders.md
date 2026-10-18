# Builder index

Builders configure creation; they do not enable device features, record commands
or synchronize resource use. Use the typed flat aliases such as
`vkz::buffer_usage_storage_upload` and `vkz::shader_stage_compute` for flags.
Examples place every chained call on its own line and indent configuration under
its section selector. Include `<vulkanizer/builders.hpp>` for pipeline and
descriptor-layout factories, plus the resource/context headers used below.

## Context

Include `<vulkanizer/context.hpp>`. `context::builder()` returns `vkz::builder`:

```cpp
auto owned_context =
    vkz::context::builder()
        .app_name("example")
        .app_version("1.0.0")
        .api_version(VK_API_VERSION_1_3)
        .add_queue(vkz::queue_graphics)
        .build();
```

This is a context without a presentation surface. For windowed rendering, supply
the window system's instance extensions and a `surface_provider` through
`surface()` before building. See [application setup](application.md) for the
GLFW-based application layer.

| Methods | Configuration |
|---|---|
| `app_name()`, `app_version()`, `engine_name()`, `engine_version()` | Application/engine metadata; versions are strings |
| `api_version()` | Requested Vulkan API version |
| `add_instance_extension()`, `add_instance_layer()` | Instance requirements |
| `add_device_extension()`, `add_device_layer()` | Device requirements |
| `enabled_features()` | Core `VkPhysicalDeviceFeatures` |
| `add_extension()`, `add_extension_chain()` | Device feature/extension structures |
| `add_queue()`, `add_unique_queue()`, `num_graphics_queues()` | Queue requirements |
| `surface()`, `device_factory()` | Surface creation and custom device selection |
| `queue_flags()`, `unique_queue_flags()`, `graphics_queue_count()` | Inspect queue requests |
| `build()` | Create the context |

Request supported features/extensions explicitly before creating dependent
resources. The context is move-only and manages its owned Vulkan setup. Finish
GPU work and release dependent objects before its destruction. Keep externally
supplied factory/provider objects valid for the build operation.

## Swapchain

Include `<vulkanizer/swapchain.hpp>`. `window_context` below is a live context
with a presentation surface and suitable queues. Choose dimensions, image count,
format, color space and usage supported by that surface/device.

```cpp
auto presentation =
    vkz::swapchain::builder(window_context)
        .set_min_image_count(2)
        .set_image_format(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        .set_image_usage(vkz::image_usage_color)
        .set_extent(1280, 720)
        .set_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .build();
```

The builder type is `swapchain::builder_base`. It also provides
`set_pre_transform(surface_transform)` and `set_composite_alpha(composite_alpha)`.
The swapchain is move-only and cleans up its owned objects. It retains a reference
to its context: keep that context at the same address and alive for its lifetime.
Recreate presentation resources when the surface changes; the builder does not
implement a rendering loop or automatic resize handling.

## Descriptor-set layouts

Use `make_descriptor_set_layout_builder(device)` or construct
`descriptor_set_layout_builder` directly. Each `binding(index)` starts a nested
binding description and commits the preceding binding. Set a positive descriptor
count and the shader visibility for every binding.

```cpp
auto descriptor_layout =
    vkz::make_descriptor_set_layout_builder(device)
        .name("simulation")
        .binding(0)
            .descriptor_type(vkz::descriptor_type_storage_buffer)
            .descriptor_count(1)
            .shader_stages(vkz::shader_stage_compute)
        .binding(1)
            .descriptor_type(vkz::descriptor_type_combined_image_sampler)
            .descriptor_count(1)
            .shader_stages(vkz::shader_stage_fragment)
        .create_layout();
```

`create_layout(flags)` commits the final binding and returns a
`vkz::descriptor_set_layout`; flags default to `descriptor_layout_flags_none`.
Call its `destroy()` once after its Vulkan lifetime requirements are satisfied.
It is a copyable handle wrapper and does not clean up on scope exit.

The nested binding builder's `build()` has a different result: it returns a
`std::vector<VkDescriptorSetLayoutBinding>`, without creating a Vulkan layout.
Both terminal operations append the pending binding, so use one terminal operation
per configured sequence. Keep the parent builder alive while using its binding
proxy, and while consuming returned bindings that refer to immutable-sampler
storage. `immutable_sampler()` and `immutable_samplers()` copy sampler handles;
keep the samplers alive for their required Vulkan lifetime.

Call `bindless()` before selecting bindings to request the builder's bindless
binding flags and update-after-bind layout flag. Enable the corresponding device
features and use a compatible descriptor pool. This opt-in does not enable those
features itself.

Layout declarations explicitly specify a descriptor type. Descriptor **writes**
are a separate API and infer the type from the concrete descriptor supplied;
this builder does not change that inference.

## Resource and pipeline builders

| Builder | Entry point | Guide |
|---|---|---|
| `buffer_builder` | `buffer::builder(allocator)` / `allocator.make_buffer_builder()` | [Buffers](resources.md#buffers) |
| `image_builder` | `image::builder(allocator)` / `allocator.make_image_builder()` | [Images](resources.md#images) |
| `image_view_builder` | `image_view::builder(device)` | [Image views](resources.md#image-views-and-samplers) |
| `sampler_builder` | `sampler::builder(device)` | [Samplers](resources.md#image-views-and-samplers) |
| `graphics_pipeline_builder` | `make_graphics_pipeline_builder(device)` | [Graphics](pipelines.md#graphics) |
| `compute_pipeline_builder` | `make_compute_pipeline_builder(device)` | [Compute](pipelines.md#compute) |
| `ray_tracing_pipeline_builder` | `make_ray_tracing_pipeline_builder(allocator)` | [Ray tracing](pipelines.md#ray-tracing) |

Textures compose an image, view and sampler; they have no separate builder.
See [textures](resources.md#textures) for composition and file loading.

Graphics state uses nested builders for shader stages, vertex input, input assembly,
tessellation, viewport/scissor, rasterization, multisampling, depth/stencil,
color blending, dynamic state, layout and dynamic-rendering formats. Shader,
viewport, scissor, stencil, blend-attachment, blend-factor, blend-operation and
logic-operation proxies configure their parent state; they are not standalone
Vulkan resources. Compute has its own shader-stage and layout builders.
Ray tracing has named shader groups and its layout section. See the
[pipeline state table](pipelines.md#graphics-state-sections) and
[ray-tracing DSL](ray_tracing.md) for these nested sections.

Keep nested proxies within their parent's lifetime. For pointer-taking APIs,
keep arrays and extension chains valid through the creation call. A copied
`create_info()` is not an owning snapshot of its pointer-backed data.
