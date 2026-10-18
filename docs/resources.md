# Buffers, images and textures

Include `<vulkanizer/memory.hpp>` for buffers, images, image views, samplers and
their builders. Include `<vulkanizer/texture.hpp>` for textures and file loading.
See [pipeline builders](pipelines.md) and the [builder index](builders.md) for
the objects that consume these resources.

The examples assume an initialized `vkz::context context`, a
`vkz::device device`, and a `vkz::vma_memory_allocator allocator`. Create an
allocator from a context with `vkz::vma_memory_allocator::create(context)`.
Keep the instance and device alive until the allocator and all its allocations
have been destroyed. `create_not_owned(existing_vma_allocator)` wraps an external
VMA allocator; destroying that wrapper does not destroy the external allocator.

## Buffers

`vkz::buffer` holds a `VkBuffer`, its creation information and its VMA allocation.
Build a GPU buffer with a size in **bytes** and the operations it must support:

```cpp
auto storage =
    vkz::buffer::builder(allocator)
        .size(4096)
        .usage(vkz::buffer_usage_storage_transfer)
        .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
        .name("simulation_state")
        .build();
```

`allocator.make_buffer_builder()` and `vkz::buffer_builder{allocator}` are
alternative entry points to the same builder. Its defaults are a one-byte
storage buffer, exclusive sharing and `VMA_MEMORY_USAGE_AUTO`. Set the size and
memory policy explicitly for application resources.

Usage flags describe permitted GPU operations; they do not allocate descriptors,
upload data or insert synchronization. Flat aliases retain their enum type and
can be combined with `|`:

| Usage | Meaning |
|---|---|
| `buffer_usage_storage` / `buffer_usage_uniform` | Storage / uniform buffer |
| `buffer_usage_vertex` / `buffer_usage_index` | Vertex / index input |
| `buffer_usage_transfer_src` / `buffer_usage_transfer_dst` | Copy source / destination |
| `buffer_usage_storage_upload` | Storage plus transfer destination |
| `buffer_usage_storage_download` | Storage plus transfer source |
| `buffer_usage_storage_transfer` | Storage plus both transfer directions |
| `buffer_usage_vertex_upload` | Vertex input plus transfer destination |

The `_upload`, `_download` and `_transfer` suffixes describe transfer usage bits,
not CPU visibility. Select host-visible memory separately when mapping.

### Host writes and readback

This example creates a small host-visible uniform buffer. `constants` is an
application-defined, trivially copyable value whose layout matches the shader.

```cpp
auto uniform =
    vkz::buffer::builder(allocator)
        .size(sizeof(constants))
        .usage(vkz::buffer_usage_uniform)
        .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
        .build();

auto mapped = uniform.map();
std::memcpy(mapped.as<void>(), &constants, sizeof(constants));
uniform.flush(0, sizeof(constants));
mapped.unmap();
```

Include `<cstring>` for `std::memcpy`. Mapping is explicit: `vkz::mapping` does
not automatically unmap on destruction. Flush host writes before GPU use; for
readback, wait for GPU completion and invalidate the range before reading it.
Mapping, flushing and invalidating do not wait for GPU work or replace barriers.
Do not modify a buffer while an outstanding submission is using that range.

### Buffer builder reference

| Method | Configuration |
|---|---|
| `flags(buffer_flags)` | Buffer creation flags |
| `size(bytes)` | Allocation size |
| `usage(buffer_usage)` | Permitted Vulkan buffer operations |
| `sharing_mode(mode)` | Exclusive or concurrent queue sharing |
| `queue_families(indices, count)` | Queue-family array for concurrent sharing |
| `memory_usage(policy)` | VMA memory selection policy |
| `next(chain)` | Borrowed Vulkan `pNext` chain |
| `name(text)` | Debug name |
| `create_info()` | Read-only `VkBufferCreateInfo` view |
| `build()` | Allocate and return a buffer |

The allocator, queue-family array and any `pNext` structures must remain valid
through `build()`. The builder stores pointers to the array and chain, not copies.
Each `build()` creates another allocation; the builder does not own the results.

## Images

`vkz::image` owns an allocation for image storage. Image views and samplers are
separate objects. A storage image that will also be sampled can be created as:

```cpp
auto output =
    vkz::image::builder(allocator)
        .type(VK_IMAGE_TYPE_2D)
        .format(VK_FORMAT_R16G16B16A16_SFLOAT)
        .extent(1280, 720)
        .mip_levels(1)
        .array_layers(1)
        .samples(vkz::sample_count_x1)
        .tiling(VK_IMAGE_TILING_OPTIMAL)
        .usage(vkz::image_usage_sampled_storage)
        .initial_layout(VK_IMAGE_LAYOUT_UNDEFINED)
        .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
        .build();
```

The image builder defaults to a 1×1×1 2D RGBA8 UNORM image, one mip, one layer,
one sample, optimal tiling, sampled/upload usage, exclusive sharing, undefined
initial layout and GPU-only memory. `allocator.make_image_builder()` is another
entry point. Choose a format whose device capabilities support the requested
usage and sample count.

`image_usage_color_sampled` combines a color attachment with sampling;
`image_usage_depth_stencil_sampled` combines depth/stencil attachment usage with
sampling. The same upload/download/transfer suffix rules apply as for buffers.

`initial_layout()` specifies creation state; it does not record a transition.
Record the required barriers before storage writes, sampling or attachment use.
The public `image.layout` field is bookkeeping, not a query of GPU state. Keep
it consistent when using raw Vulkan commands alongside library helpers.

| Image builder method | Configuration |
|---|---|
| `flags(image_flags)` | Creation flags, including cube compatibility |
| `type(type)`, `format(format)` | Image dimensionality and pixel format |
| `extent(width, height, depth)` / `extent(VkExtent3D)` | Dimensions; depth defaults to 1 |
| `mip_levels(count)`, `array_layers(count)` | Storage for mip levels and array layers |
| `samples(sample_count)`, `tiling(tiling)` | Sample count and tiling |
| `usage(image_usage)` | Permitted image operations |
| `sharing_mode(mode)`, `queue_families(indices, count)` | Queue-family sharing |
| `initial_layout(layout)`, `memory_usage(policy)` | Creation layout and VMA policy |
| `next(chain)`, `create_info()`, `build()` | Extension chain, inspection and allocation |

Allocating mip levels does not generate their contents. Queue-family arrays and
extension chains have the same borrowed lifetime requirements as buffer builders.

## Image views and samplers

An image view selects the format interpretation, aspect, mip levels and layers
that a descriptor or rendering attachment uses:

```cpp
auto output_view =
    vkz::image_view::builder(device)
        .image(output)
        .view_type(VK_IMAGE_VIEW_TYPE_2D)
        .aspect_mask(vkz::image_aspect_color)
        .base_mip_level(0)
        .level_count(1)
        .base_array_layer(0)
        .layer_count(1)
        .build();
```

`.image(output)` copies the image handle and format. It does not infer the view
type, depth/stencil aspect or full mip/layer range. The defaults select color,
one mip and one layer of a 2D view. Use `image_aspect_depth`,
`image_aspect_stencil` or `image_aspect_depth_stencil` as appropriate.
`format()`, `components()` and `subresource_range()` override individual
interpretation or range choices; `flags()`, `next()` and `create_info()` are also
available. `image_view::build(device, image)` is a one-level color-view shorthand.

```cpp
auto linear_sampler =
    vkz::sampler::builder(device)
        .mag_filter(VK_FILTER_LINEAR)
        .min_filter(VK_FILTER_LINEAR)
        .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
        .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
        .min_lod(0.0f)
        .max_lod(0.0f)
        .build();
```

Sampler defaults are linear filtering/mipmap filtering, repeat on U/V/W, minimum
LOD zero and no maximum LOD clamp. Anisotropy, comparison and unnormalized
coordinates are disabled. `address_mode()` sets all three axes; the `_u`, `_v`
and `_w` variants set them separately. Other controls are `mip_lod_bias()`,
`anisotropy(enabled, max)`, `compare(enabled, op)`, `border_color()`,
`unnormalized_coordinates()`, `flags()` and `next()`.
Enable the corresponding device features before requesting anisotropy or other
feature-dependent settings. A sampler does not own or reference a particular image.

## Textures

`vkz::texture` groups an image, its view and a sampler. There is no separate
texture builder: compose the three resource builders or load a file.

To transfer ownership of the components above into one texture:

```cpp
vkz::texture result{
    .image = output,
    .image_view = output_view,
    .sampler = linear_sampler,
};
```

These assignments copy handles, not allocations. Treat `result` as the sole
owner from this point and do not separately destroy the original component
wrappers. A texture used as a descriptor still needs a matching descriptor layout
and the correct image layout when the shader accesses it.

For a file, supply an upload queue and its queue-family index:

```cpp
auto albedo =
    vkz::load(
        allocator,
        upload_queue,
        upload_queue_family,
        "albedo.png",
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_SAMPLER_ADDRESS_MODE_REPEAT);
```

`load()` submits the upload and waits for it to finish before returning. Ordinary
images are decoded as RGBA, uploaded with one mip level and transitioned to
shader-read-only layout. Use `VK_FORMAT_R8G8B8A8_SRGB` for encoded color,
`VK_FORMAT_R8G8B8A8_UNORM` for linear byte data, or
`VK_FORMAT_R32G32B32A32_SFLOAT` for floating-point decoding. Other supplied formats
must match the decoded RGBA representation. The optional final `flip_uv` argument
flips ordinary decoded images vertically.

`.ktx` and `.ktx2` files go through the KTX loader instead, using their stored
format and mip metadata; the ordinary-image `format` and `flip_uv` choices do not
override that path. Use an upload queue capable of the commands involved and
arrange queue ownership and visibility if rendering uses a different queue.
Avoid calling this synchronous loader in a frame's hot path.

## Cleanup

These resource wrappers use explicit cleanup, not automatic scope-based cleanup:

```cpp
// After all GPU work using these resources has completed:
albedo.destroy();
result.destroy();
uniform.destroy();
storage.destroy();
```

`texture::destroy()` destroys sampler, view and image in that order. For resources
managed individually, destroy views before their images, then release allocations
before the allocator, and the allocator before the device/context. Do not destroy
copied wrappers twice. `allocator.deallocate(buffer_or_image)` takes a copy and
does not clear the caller's handles; use one cleanup path per allocation.

For repeated asynchronous uploads, `vkz::staging_buffer` provides aligned borrowed
ranges and records buffer copies through `borrowed_memory::copy_to()`. Keep the
borrow and staging allocation alive until the copy finishes, then explicitly
`return_memory()`. See its declarations in `memory.hpp` for the transfer overloads.
