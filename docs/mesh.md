# Mesh rendering data

`vulkanizer/mesh.hpp` provides a format-independent static mesh representation.
Importers convert source geometry into `data::meshes` and place it using
`data::instances`. Materials follow the core glTF 2.0 metallic-roughness model.
This is CPU asset data, not a shader buffer layout or a glTF parser.

## Materials

`material` contains linear `base_color_factor` (RGBA), `emissive_factor` (RGB),
`metallic_factor`, `roughness_factor`, `normal_scale`, `occlusion_strength`,
`alpha_mode`, `alpha_cutoff`, and `double_sided`. Defaults match core glTF:
white base color, no emission, metallic and roughness factors of one,
normal scale and occlusion strength of one, opaque alpha, cutoff 0.5,
and single-sided rendering. Shadow flags are independent renderer settings.

| Texture slot | Channel interpretation |
| --- | --- |
| `base_color_texture` | RGB decoded from sRGB; linear alpha coverage |
| `metallic_roughness_texture` | Linear G = roughness, B = metallic |
| `normal_texture` | Linear RGB tangent-space normal, scaled in XY by `normal_scale` |
| `occlusion_texture` | Linear R, modulates indirect light using `occlusion_strength` |
| `emissive_texture` | RGB decoded from sRGB |

Color-space interpretation belongs to the slot, not the image: one image can
be used by both color and data textures. Factors are linear. Base color alpha
is not premultiplied. Opaque mode ignores alpha; mask mode compares alpha with
`alpha_cutoff`; blend mode uses alpha coverage. Double-sided shading must disable
culling and reverse the back-face shading normal.

Separate source opacity maps must be combined into base-color alpha by the
importer. Other source shading models must be converted to metallic-roughness.
Transmission, clearcoat, sheen, animation, and BRDF shader evaluation are not
implemented by these data structures.

## Texture resources

Each slot is a `texture_info` with an `index` into `data::textures` (`-1` means
absent), `tex_coord`, `offset`, `scale`, and `rotation` in radians. Apply the UV
transform as `offset + rotationMatrix * (scale * uv)`. The importer resolves any
`KHR_texture_transform` UV-set override into `tex_coord`.

A `texture` references `data::images` and optionally `data::samplers`.
`sampler_id == -1` selects the documented runtime default: linear magnification,
linear minification and mip interpolation, repeat wrapping, and mipmaps enabled.
glTF leaves omitted min/mag filter choices to the implementation. Use
`use_mipmaps == false` for non-mipmapped minification; the renderer must clamp
sampling to mip zero in that case. The two filters and `mipmap_mode` represent
all six glTF minification modes.

An `image` owns either an asset URI or encoded bytes with a MIME type.
Importers must normalize external references to stable asset URIs so paths
remain meaningful when assets from different source directories are merged.
Converted images contain complete KTX2 payloads, with Vulkan formats and mipmaps;
PNG/JPEG decoding happens during conversion. Exactly one of `uri` and
`encoded_data` must be populated. GPU objects are created when uploading.

## Instances and merging

`instance::mesh_id` selects a base mesh. `material_id == inherit_material` uses that
mesh's `material_id`; other values override it. Importers supply a default material
when the source has none. World matrices and inverse matrices must agree.

`merge_mesh_data` preserves this sentinel and remaps explicit material IDs, mesh
IDs, texture slots, image IDs, and sampler IDs. Shared resources within a source
remain shared; resources across sources are appended without deduplication.

## Storage

Use `.vkmesh` for geometry, mesh instances, placed lights and embedded KTX2 images;
`.vkmat` for materials, sampler settings and texture references; and `.vkbounds`
for separately stored local bounding boxes. Geometry
files also contain those boxes. File APIs accept `const vkz::path&`.

Geometry format version 4 includes a count and field-wise light records after
the mesh instances. Each light record stores its type, color, intensity, range,
world position, world direction and spot cone angles (56 bytes). Light placement
and properties are stored together; there is no separate light-instance table
or lighting file. Image records follow the lights and store URI, MIME type and
length-prefixed encoded bytes. Versions 1â€“3 remain readable; versions 1â€“2 have
no lights. Loading version 4 replaces images and lights; material version 2
preserves them. A legacy material version 1 file still loads its own images.
Merging data appends placed lights without changing their world positions.
Light-only data can be saved, loaded and merged with geometry.

Geometry magic is `0x475A4B56` (`VKZG`, Vulkanizer geometry, in little-endian
byte order). The cookbook magic `0x12345678` is rejected; old caches using it
must be reimported. The bundled converted Sponza cache uses the new magic.

Material files begin with `VKZM` magic and version 5, then contain materials,
samplers, texture references and the expected image count, without image payloads.
Material fields and resource records are written
individually, rather than dumping structs containing vectors or GLM padding.
The cache uses native-endian 32-bit scalar values and is not a cross-endian
interchange format. Legacy unversioned cookbook material caches are rejected
and must be reimported. Geometry and material files remain separate.

Load/save validates resource references and core material values. Failed loads
and merges leave the destination unchanged. Image payload validity and the
availability of each requested UV set are checked by the decoder/importer,
not by the material-file reader.

## Conversion

```cpp
#include <vulkanizer/mesh.hpp>

// Writes assets/converted/bistro.vkmesh, .vkmat, and .vkbounds.
auto files = vkz::mesh::convert("assets/bistro.glb", "assets/converted/bistro");
// Omit the output base to write alongside the input with its original stem.
vkz::mesh::data scene;
vkz::mesh::load_mesh_data(files.mesh_file, scene);
vkz::mesh::load_mesh_data_materials(files.material_file, scene);
```

The converter dispatches by a case-insensitive source extension; `.gltf` and
`.glb` use tinygltf. Additional importers can be added to the dispatch table in
`src/mesh_convert.cpp` and return the same `data` model. Output parent directories
are created, and existing output files are overwritten. Conversion throws on
unsupported or invalid input and on I/O failures. Writes of the three files are
not an atomic transaction; an I/O failure may leave partial output.

Texture conversion uses the Khronos [ktx create](https://github.khronos.org/KTX-Software/ktxtools/ktx_create.html) executable. Install KTX-Software
and put `ktx` on PATH, or set `VKZ_KTX_EXECUTABLE` to its executable path. This is
an offline conversion dependency; runtime loading only uses libktx. Temporary
input/output files are removed after conversion. The resulting KTX2 files are
embedded in `.vkmesh`; no texture sidecars are needed. Output uses uncompressed
RGBA8 with a full mip chain, avoiding runtime image decoding and transcoding.
Base-color/emissive uses sRGB, other slots use linear UNORM. If an image is used
in both roles, separate variants are generated and material references remapped.
Encoded variants are shared across sampler combinations; unused images are omitted.

The memory overload of `vkz::texture_from_ktx` reads embedded bytes and applies
the saved sampler description, including independent S/T wrapping, filters and
non-mipmapped LOD clamping:

```cpp
#include <vulkanizer/ktx.hpp>
const auto& ref = scene.textures[texture_index];
const auto settings = ref.sampler_id < 0 ? vkz::mesh::sampler{}
                                       : scene.samplers[ref.sampler_id];
auto texture = vkz::texture_from_ktx(scene.images[ref.image_id].encoded_data,
                                    allocator, queue, queue_family, settings);
```

Vulkan image, view and sampler handles are still created at runtime; GPU handles
cannot be serialized for reuse across runs.

The default scene is imported, or the first scene when no default is specified.
If scenes are absent, root nodes are traversed. Each referenced glTF primitive
becomes one shared base mesh; each node referring to it adds an instance. World
transforms include parents, and inverse transforms are stored. Geometry stays
in object space, including for mirrored instances; the renderer must account
for negative transform determinants when handling winding and tangent frames.

`KHR_lights_punctual` is supported, including when listed as required. Each light
node in the selected scene creates one `data::lights` entry, even if several
nodes reference the same source definition. A node may contain both a mesh and
a light. Scenes containing only lights are also supported.

`light_type` supports `directional`, `point`, and `spot`. Colors are linear RGB;
intensity uses candela for point/spot lights and lux for directional lights.
Point/spot positions include parent transforms. Directional/spot directions are
the normalized transformed local -Z axis. Scale affects placement but does not
rescale intensity, range, or cone angles. A runtime range of zero means
unbounded; explicit glTF ranges must be positive. Spot inner/outer half-angles
are stored in radians. The renderer should apply inverse-square attenuation to
point/spot lights, enforce finite range cutoffs, and apply spot angular falloff.
Directional lights do not attenuate with distance. Light import and storage do
not add lighting shaders or shadow rendering.

Triangle lists, strips and fans become indexed triangle lists. Accessors support
interleaved strides, byte offsets, sparse values, normalized unsigned colors/UVs,
and unsigned 8/16/32-bit indices. Missing normals generate flat normals by
splitting vertices. Supplied tangents retain handedness; absent tangents are zero
(including `w`), indicating that the renderer must derive a tangent frame from
triangle positions and the normal texture's UV set. The importer does not
generate MikkTSpace tangents.

The output has one interleaved float vertex binding. Its attributes are:

| Location | Attribute | Format | Byte offset |
| --- | --- | --- | --- |
| 0 | Position | float3 | 0 |
| 1 | Normal | float3 | 12 |
| 2 | Tangent and handedness | float4 | 24 |
| 3 | Color 0 (white if absent) | float4 | 40 |
| 4 + N | TEXCOORD_N | float2 | 56 + 8 Ã— ordinal of N |

UV sets are sorted by their numeric suffix; the stride is 56 plus eight bytes
per set. Sets missing from a primitive are zero-filled, but a material referring
to a missing set is rejected. Read `data::streams` for the actual layout. UVs
and image bytes are not flipped. The renderer must respect the target device's
vertex attribute limits for models with many UV sets.

Core metallic-roughness materials, alpha modes, double-sided rendering flags,
all 17 texture slots, samplers, and `KHR_texture_transform` are preserved.
External images, data URI images, and GLB image buffer views are converted to
KTX2 payloads embedded in `.vkmesh`, so converted files do not depend on the
original image paths.

Animation tracks are ignored, using the static node transforms. Skins, morph
targets, non-triangle geometry, additional color sets, Draco geometry and
unsupported required extensions are rejected. Optional extensions outside the supported list in the reference guide use the core glTF fallback; cameras are not imported. Singular mesh transforms
are rejected because instances require an inverse. Directional/spot transforms
must provide a nonzero emission axis.

References: [glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
and [KHR_texture_transform](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_texture_transform).
Lighting follows [KHR_lights_punctual](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual).

## Sponza reference rendering test

`vulkanizer_sponza_test` loads `test/mesh/resources/sponza/Sponza.vkmesh` and its
`.vkmat` companion. The default pass renders core glTF metallic-roughness
materials. Use `--wireframe` for the original white lines. Both use the blue clear
color, instance transforms, and the FPS camera.

Controls: WASD to move, Q/E down/up, hold left mouse to look, wheel to zoom,
and Escape to exit. The window is fixed at 1280 by 800.

```powershell
cmake --build cmake-build-debug --config Debug --target vulkanizer_sponza_test vulkanizer_gltf_brdf_test
cmake --build cmake-build-release --config Release --target vulkanizer_sponza_test vulkanizer_gltf_brdf_test
./cmake-build-debug/test/Debug/vulkanizer_sponza_test.exe
./cmake-build-release/test/Release/vulkanizer_sponza_test.exe --validation --timeout 3 --screenshot sponza.png
```

The shaders compile to SPIR-V at build time and are embedded by CMakeRC 2.0.1
(commit `9a80e17c75fca503a55a38ea011dcfaf2a830cc9`). The vendored script and MIT
license are under `cmake/`. `src/reference.cpp` opens the library cmrc filesystem
and copies the bytes into aligned SPIR-V words. The pipeline factory and its shader
resources are built even with tests disabled, and are part of `vulkanizer.lib`. No runtime shader paths or GLSL
compiler are needed. Scene resources remain external.

### Core material behavior

- The reusable `src/reference/gltf_core.glsl` implements the glTF
  Appendix B metallic/dielectric mixture, GGX distribution, correlated Smith
  visibility, Schlick Fresnel, and Lambert diffuse. Dielectric F0 is derived from IOR (0.04 at the default 1.5);
  microfacet alpha is squared perceptual roughness, clamped to 0.001 for finite
  raster highlights at zero roughness.
- Base color factor, texture, and linear vertex color multiply together.
  Embedded KTX formats provide hardware sRGB decoding for base-color/emissive
  RGB; alpha remains linear. Roughness is G, metallic is B, and occlusion is R.
- All 17 texture slots use their selected UV set and saved sampler. Vertex
  pulling reads the canonical importer layout and selects UV offsets from the
  stream metadata without a fixed TEXCOORD_0/1 limit.
- Normals use inverse-transpose transforms. Supplied tangents are transformed
  and orthogonalized; mirrored transforms adjust handedness and front-face
  state. Without tangents, a derivative frame is used (an approximation, not
  MikkTSpace tangent generation). Normal scale affects XY before normalization.
- OPAQUE ignores alpha; MASK uses alpha cutoff; BLEND uses straight-alpha
  blending with depth writes disabled. Transparent instances are sorted back
  to front by view-space bounds center. Intersecting transparent geometry still
  needs a more advanced ordering solution.
- Single-sided back faces are discarded; double-sided back-face normals are
  reversed. Emissive is added independently of direct light and occlusion.
- The viewer supplies a directional headlight and a small diffuse ambient term.
  Occlusion scales only that ambient contribution. Output is linear into an
  HDR target. A separate tone-mapping pass precedes sRGB encoding.

IOR, transmission via optional ray queries, optional IBL, and tone mapping are described in the [reference integration guide](reference_ray_tracing.md). Texture transforms, punctual lights and the material extensions listed in the reference guide are evaluated in both pipelines. This preview has no shadows, diffuse GI, animation, or volume scattering.

`vulkanizer_gltf_brdf_test` evaluates the shared GLSL on the GPU and checks
analytic normal-incidence values, metallic interpolation, reciprocity,
zero-roughness/grazing stability, and the unlit hemisphere. These checks use
runtime failures in both Debug and Release. Sponza's timed test checks the
integrated rendering path when the local converted asset is available.

The previous `gltf_viewer` shader was consulted, with behavior cross-checked
against [glTF materials](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#materials)
and [Appendix B](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation).


## Library reference pipeline

Include `<vulkanizer/reference.hpp>` and link `vulkanizer::vulkanizer`.
`vkz::reference::create_pipeline` creates a graphics pipeline and its pipeline
layout. Its implementation and embedded shaders are in `src/reference.cpp` and
`src/reference/`, and are available with tests disabled. Clients retain their
own renderer, uploads, descriptor pools/sets, frame synchronization and draws.

```cpp
auto pipeline = vkz::reference::create_pipeline({
    .device = device,
    .layouts = {scene_layout, material_layout},
    .extent = {width, height},
    .color_format = color_format,
    .depth_format = depth_format,
    .alpha_blend = false,
    .mirrored = false,
});
pipeline.destroy();
```

The public shader ABI supplies `scene_uniform`, `material_uniform`, and
`instance_uniform` plus descriptor binding declarations:

| Set | Binding | Resource |
| --- | --- | --- |
| 0 | 0 | `scene_uniform`, std140 UBO, vertex + fragment |
| 0 | 1 | Canonical interleaved float vertices, readonly SSBO, vertex |
| 0 | 2 | `light_uniform[]`, readonly SSBO, fragment |
| 1 | 0 | `material_uniform`, std140 UBO, vertex + fragment |
| 1 | 1–17 | Combined image samplers, in the reference guide's material slot order |

`scene_uniform::vertex_layout.x` is vertex stride in float elements. Vertex
pulling expects position, normal, tangent and color at byte offsets 0, 12, 24
and 40. `material_uniform::textures[i]` contains the selected UV offset in float
elements in x and texture presence (0/1) in y. Bind valid fallback descriptors
for absent textures. `flags.x` is alpha mode (0 opaque, 1 mask, 2 blend), and
`flags.y` is double-sided. `emissive_metallic` packs emissive RGB and metallic factor;
`roughness_normal_occlusion_cutoff` packs roughness, normal scale, occlusion
strength and alpha cutoff in that order. Push `instance_uniform` to both vertex and fragment stages;
`normal_matrix` is the transpose of the model inverse.

The factory uses dynamic rendering, triangle topology, depth testing and a
fixed viewport/scissor. Configure sample count to match the attachments. Output
is linear: use an sRGB target or a linear HDR target with client tone mapping.
Select `alpha_blend` for BLEND materials (straight alpha, depth writes disabled)
and `mirrored` for negative-determinant instance transforms. The client sorts
transparent geometry and selects variants per draw. Sidedness is handled in
the fragment shader using material flags.

`vkz::reference::shader(shader_stage::vertex/fragment)` returns aligned embedded
SPIR-V for clients building a custom pipeline. No cmrc headers or shader paths
are required by clients. The Sponza renderer is test-only code in
`test/mesh/gltf_reference.hpp`, demonstrating resource uploads and draw submission
using this public pipeline API. The standalone `test/mesh/reference_client` project
uses `find_package(vulkanizer)` to verify installed shaders and all four pipeline
variants without constructing a renderer or allocating scene resources.

The Sponza example keeps `init` as a short sequence of private initialization
methods for uniforms, fallback textures, texture uploads, descriptor layouts,
descriptor pools, scene/material descriptor sets and pipeline creation.

## Mesh BVH

`<vulkanizer/bvh.hpp>` exposes `vkz::mesh::bvh`. It creates one Vulkan BLAS per `data::meshes` entry and a TLAS containing `data::instances` in their original order. The instance custom index is the index in `data::instances`. All indices are `uint32_t`; vertex and index offsets and the requested LOD come from each base mesh. Positions must be float3 at location zero in one interleaved, per-vertex stream. The position offset and vertex stride come from `data::streams`.

```cpp
auto allocator = vkz::vma_memory_allocator::create(
    context, VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
vkz::mesh::bvh acceleration{allocator};
vkz::mesh::geometry_buffers geometry{vertex_buffer, index_buffer};
acceleration.build(command_buffer, scene, geometry);
auto tlas = acceleration.tlas();
auto first_blas = acceleration.blas(0);
```

Enable `bufferDeviceAddress`, `accelerationStructure`, synchronization2, `VK_KHR_acceleration_structure`, and `VK_KHR_deferred_host_operations` on the device. Enable ray-query or ray-tracing-pipeline features separately for the client's chosen rendering path. Both geometry buffers need `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` and `VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR`, and must contain the scene's complete vertex/index arrays starting at byte zero. Any allocator used for these buffers must support device addresses.

`build` records commands into a recording command buffer outside a render pass on a compute-capable queue. It does not submit or wait. The client owns geometry uploads, queue submissions, queue-family ownership transfers, descriptors, pipelines, and shader binding tables. Uploaded geometry must agree with the supplied CPU data. Recorded barriers cover preceding writes on the same queue, sequential scratch reuse, BLAS-to-TLAS dependencies, and subsequent acceleration-structure reads. Cross-queue dependencies remain the client's responsibility.

The BVH owns its AS storage, scratch buffer, and instance upload buffer. Keep the allocator and device alive until the BVH is destroyed. Keep geometry buffers alive until builds complete. Wait for all commands using the current BVH to finish before rebuilding, updating, moving over an existing BVH, calling `destroy`, or destroying it. A rebuild replaces the AS handles; clients must refresh descriptors. Ordinary instance updates keep the TLAS handle stable.

`bvh_build_options` selects `lod` (default zero) and `allow_instance_updates` (default true). `update_instances` changes transforms, material overrides, masks, and SBT offsets. It requires the same instance count, mesh assignments, and mesh/stream layout. Geometry content changes require a full build; they are not detected by `update_instances`. Wait for any previous build/update and traversal before writing another update, because the instance upload buffer is reused. Empty scenes and meshes without indexed triangles are rejected.

An optional `std::span<const bvh_instance_options>` provides one mask and 24-bit SBT record offset per instance. An empty span uses mask `0xff` and offset zero, including during updates. Material overrides determine opacity and double-sided culling. Opaque instances are forced opaque; masked and blended instances remain nonopaque so client shaders can decide whether to accept hits. Double-sided materials disable face culling. Instance transforms retain object-space facing, including mirrored placements, as specified by [Vulkan instance flags](https://docs.vulkan.org/refpages/latest/refpages/source/VkGeometryInstanceFlagBitsKHR.html). Single-sided culling is selected by the client's ray flags.

This implementation supports static BLAS geometry and TLAS instance updates. It does not compact structures or update deforming geometry. GPU regression tests exercise the BVH through the reference ray-tracing pipeline.

### Material version 3

Each material record appends `ior` (float32), `transmission_factor` (float32), and `transmission_texture` (28-byte texture-info record) after the version-2 fields. Records are 236 bytes instead of 200. Versions 1 and 2 remain readable with IOR 1.5, zero transmission and no transmission texture. New saves use version 5, described below. IOR must be finite and at least 1, or the special value 0; transmission must be finite in [0, 1]. Transmission textures use linear KTX variants and retain their UV set/transform metadata. Reconvert old assets to import these extension values; loading an old cache cannot recover discarded source parameters.

### Material version 4

A material appends thickness factor (float32), attenuation distance (float32, positive infinity allowed for the default), attenuation color (three float32 values), and thickness texture info (28 bytes) after its version-3 fields. Each record is 284 bytes. Versions 1 through 3 remain readable with zero thickness, infinite attenuation distance, white attenuation color and no thickness texture. Thickness is finite and nonnegative; attenuation distance is positive; attenuation RGB must be finite in [0, 1]. Thickness texture data uses a linear KTX variant and retains UV metadata; its green channel represents baked thickness for clients that use thickness maps.


### Material version 5

Records are 644 bytes. After the 284-byte version-4 record, append these fields in order: specular factor (float), specular color (float3), emissive strength (float), unlit (uint32 boolean), clearcoat factor/roughness/normal scale (three floats), sheen color (float3), sheen roughness (float), anisotropy strength/rotation (two floats), iridescence factor/IOR/minimum thickness/maximum thickness (four floats), and dispersion (float). Then append ten 28-byte texture-info records: specular, specular color, clearcoat, clearcoat roughness, clearcoat normal, sheen color, sheen roughness, anisotropy, iridescence, iridescence thickness.

Versions 1–4 retain glTF defaults for missing extension fields. Specular and sheen color textures use sRGB KTX variants; the other added slots use linear variants. `texture_slots(material)` exposes all 17 slots in shader order for validation, remapping and descriptor creation. Punctual lights remain serialized with instances in `.vkmesh`; no separate light file is introduced.
