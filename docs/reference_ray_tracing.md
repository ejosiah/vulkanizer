# Mesh glTF reference pipeline

`<vulkanizer/reference.hpp>` exposes `create_ray_tracing_pipeline` alongside the graphics `create_pipeline`. Both share core material evaluation, normal mapping, the metallic-roughness BRDF, and `make_material_uniform`. Reference shader stages, optional IBL variants and background shaders are library resources embedded with cmrc, including tests-disabled builds. Clients own their renderer.

The ray pipeline traces jittered primary rays within each pixel and averages their radiance. Each ray shades its nearest accepted surface and continues through transmissive hits, up to eight secondary rays. The default is 8 samples per pixel, with a maximum of 32; a count of 1 uses the pixel center. Lighting matches the graphics reference: imported directional, point and spot lights, an optional legacy directional light, and either ambient irradiance or optional diffuse/specular IBL. IOR and transmission share the same material evaluator in both paths. There are no shadow rays, diffuse GI bounces, or volume scattering.

Alpha MASK rejects intersections below the cutoff. Alpha BLEND uses stochastic coverage in any-hit. Advance `sampling.x` and accumulate frames in your renderer to converge fractional coverage. Accepted samples write alpha 1; misses write `background`, including its alpha, or environment radiance with alpha 1 when IBL is enabled. The pipeline does not accumulate frames. Alpha coverage is separate from transmission.

## Resources

Enable buffer device addresses, acceleration structures, ray tracing pipelines, and `VkPhysicalDeviceDescriptorIndexingFeatures::shaderSampledImageArrayNonUniformIndexing`. Use a VMA allocator with `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT`. The texture array has a fixed size specialized at pipeline creation: runtime arrays, variable descriptor counts, and partially bound descriptors are not required. The texture count must fit the device's sampler and sampled-image descriptor limits.

Create set 0 from `ray_tracing_scene_bindings`:

| Binding | Descriptor | Data |
| --- | --- | --- |
| 0 | Uniform buffer | `ray_tracing_scene_uniform` |
| 1 | Storage buffer | Canonical mesh vertex bytes |
| 2 | Storage buffer | Local `uint32_t` mesh indices |
| 3 | Storage buffer | `ray_tracing_instance_uniform[]`, in scene instance order |
| 4 | Storage buffer | `material_uniform[]`, in scene material order |
| 5 | Acceleration structure | Mesh BVH TLAS |
| 6 | Storage image | `VK_FORMAT_R32G32B32A32_SFLOAT`, in `GENERAL` layout |
| 7 | Storage buffer | `light_uniform[]`, produced by `make_light_uniforms(scene)` |

Set 1 has the single combined-image-sampler array binding returned by `ray_tracing_texture_binding(texture_count)`. Populate every descriptor: slot 0 is a valid fallback; slot `i + 1` is `mesh::data::textures[i]` with its imported sampler. Base-color, emissive, specular-color and sheen-color image views decode sRGB; other material textures use linear views. The importer distinguishes these usages.

`make_material_uniform(scene, material)` is shared with graphics. Each `textures` entry contains a UV offset in float elements, a presence flag, and a texture-array descriptor index in `.z`; `.w` enables the accompanying UV transform rows and offset. Graphics ignores `.z` and binds textures individually. Missing slots use neutral samples without reading UV data.

The canonical stream contains float3 position, float3 normal, float4 tangent, float4 color, then float2 UV sets. `vertex_stride(scene)` validates it and returns the stride in float elements. Zero tangents trigger a fallback: rasterization uses fragment derivatives; ray tracing uses triangle edges and the selected UV coordinates. Both feed the same normal-map evaluator.

```cpp
std::vector<vkz::reference::material_uniform> materials;
for (const auto& material : scene.materials)
    materials.push_back(vkz::reference::make_material_uniform(scene, material));

auto instances = vkz::reference::make_ray_tracing_instances(scene, lod);
auto pipeline = vkz::reference::create_ray_tracing_pipeline({
    .allocator = allocator,
    .layouts = {scene_layout, texture_layout},
    .texture_count = uint32_t(scene.textures.size() + 1)
});
```

Upload these arrays to storage buffers. The instance helper resolves inherited/overridden materials and LOD index offsets. Build `mesh::bvh` with the same LOD. Its instance custom indices already match this array. Keep every BVH SBT offset at zero: one `surface` hit group handles all meshes and materials.

Geometry or LOD changes require a BVH rebuild and refreshed instance data. Material assignments require refreshing instance data and calling BVH `update_instances`. Refresh material uniforms when their factors change; also update BVH flags when alpha mode, double-sidedness, or transmission changes.

## Camera and dispatch

Fill the nested `scene` as for graphics, including `vertex_layout.x = vertex_stride(scene)`. Supply `inverse_view_projection` for a finite Vulkan zero-to-one projection with the same Y flip as graphics. Rays start on the near plane; perspective and orthographic projections are supported.

`ray_parameters` holds minimum distance, maximum distance, explicit texture LOD, and a reserved component. LOD defaults to zero; ray differentials are not implemented. `sampling` holds the sampling seed, 8-bit instance mask, sample count, and the maximum secondary-bounce count (default 8, clamped to 8). The shader clamps the sample count to 1–32. Each sample uses a distinct alpha-coverage random value. Supply a nonzero light direction even when radiance is zero.

Bind both descriptor sets, transition the output to `GENERAL`, and dispatch its exact width and height with depth 1:

```cpp
vkz::bind_pipeline(command_buffer, pipeline);
vkCmdBindDescriptorSets(command_buffer, pipeline.bind_point, pipeline.layout,
                       0, uint32_t(sets.size()), sets.data(), 0, nullptr);
vkz::trace_ray(command_buffer, pipeline, width, height);
```

No push constants_ are needed. Output is linear HDR: clients perform tone mapping and display encoding. Synchronize uploads/BVH builds before tracing and ray shader writes before consuming the output. The pipeline owns its pipeline/layout and SBT; clients own other resources. Finish outstanding GPU work before destruction.

`vulkanizer_reference_ray_tracing_test` performs GPU readback checks. The installed `test/mesh/reference_client` checks both pipeline families and their embedded resources through the installed library.

Run `vulkanizer_sponza_test --ray-traced` to view Sponza using this pipeline. The default remains rasterized glTF shading; `--wireframe` selects the wireframe view and cannot be combined with `--ray-traced`. All modes share the FPS camera and lighting. Ray tracing requires the device features listed above and uses the shared HDR tone-mapping pass for presentation. `--validation`, `--timeout 3`, and `--screenshot output.png` also work in ray-traced mode. Use `--samples N` with `--ray-traced` to choose 1–32 samples per pixel (default 8). Invalid values are rejected. Samples are averaged within each frame using a fixed seed for a stable preview; the viewer does not accumulate across frames.

References: [glTF core materials](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#materials), [Vulkan ray traversal](https://docs.vulkan.org/spec/latest/chapters/raytraversal.html).


## IORTestGrid setup

`vulkanizer_ior_test_grid_test` uses the same viewer and rendering options as Sponza, with an orbit camera framing the grid. Run it normally for graphics or with `--ray-traced --samples 8` for ray tracing. Converted resources are in `test/mesh/resources/ior_test_grid`.

The converted grid includes KHR_materials_ior and KHR_materials_transmission. Volume parameters are now imported. The specular extension is supported; reconvert caches created before material version 5 to recover its source values. Asset attribution and conversion details are beside the converted files.

## Optional image-based lighting

`vulkanizer/ibl.hpp` contains the IBL filters ported from Vista. Vista retains its copy until its callers are migrated. `vkz::ibl::load(allocator, queue, queue_family, hdr_path)` uploads floating-point HDR radiance, converts the equirectangular image to a cubemap, generates cosine-convolved irradiance and GGX-prefiltered specular mips, and loads the embedded linear BRDF LUT. It submits to the supplied graphics/compute-capable queue and waits for completion. The returned `maps` owns four textures; call `destroy()` after their last GPU use. Map sizes can be supplied through `vkz::ibl::settings`.

For application-owned input/output textures, `vkz::ibl::generator` records conversion and filtering into a caller's command buffer. Keep the generator and all referenced textures alive until the commands finish. Output images must be distinct six-layer cube-compatible RGBA32F storage images; conversion and irradiance outputs have one mip, while specular mips span perceptual roughness 0 to 1. Descriptors are cached by input view and output image/mip and retained until generator destruction (up to 1,024 unique bindings per generator). Repeated filtering of the same maps reuses them. The source must already be readable by compute on the same queue family. The generator does not submit commands or transfer queue ownership. Irradiance stores the integral E, so Lambertian shading divides by pi.

Both reference pipeline create-info types accept an optional `ibl_layout`. Leaving it empty retains the existing two-set shaders and ambient approximation. Supplying it enables shared split-sum IBL at descriptor set 2:

| Binding | Type | Resource |
| --- | --- | --- |
| 0 | Combined image sampler | Environment cubemap |
| 1 | Combined image sampler | Irradiance cubemap |
| 2 | Combined image sampler | Specular cubemap with roughness mips |
| 3 | Combined image sampler | Linear BRDF integration LUT, RG = scale/bias |
| 4 | Uniform buffer | `vkz::reference::ibl_uniform` |

Use `ibl_bindings(VK_SHADER_STAGE_FRAGMENT_BIT)` for graphics, or `ibl_bindings(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR)` for ray tracing. All four images must be in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`. `ibl_uniform.parameters.x` is the environment intensity (default 1), and `.y` rotates lookup directions around world Y in radians (default 0); `.z` selects the blurred irradiance background when greater than 0.5; `.w` is reserved. Update `inverse_view_projection` and `eye` with the camera. Irradiance and prefiltered specular replace the ambient approximation; occlusion applies to both indirect terms. Direct light and emission remain separate. The environment is infinitely distant and does not provide local visibility or additional ray bounces.

The ray miss shader samples the environment. Raster clients can create `create_background_pipeline(...)` with the same IBL layout and attachment formats, bind the IBL set at **set 0** for that pipeline, and draw a fullscreen triangle with three vertices before the mesh. It tests depth at 1 without writing depth. Background and surface pipelines output linear HDR. The test viewer renders both modes into an RGBA32F image and runs the tone-mapping pass below before the sRGB swapchain write.

The IORTestGrid executable enables the studio HDR environment in graphics and ray-traced modes. `--no-ibl` disables it. IOR, transmission and volume are supported in both modes.

## Tone mapping

`create_tone_mapping_pipeline({device, layout, extent, color_format})` creates a fullscreen graphics pipeline, with its fragment shader embedded through cmrc. Create `layout` using `tone_mapping_binding`: set 0, binding 0 is a combined image sampler for the single-sample linear HDR scene image in `SHADER_READ_ONLY_OPTIMAL`. Bind the set, push `tone_mapping_uniform{exposure}` to the fragment stage at offset 0, and draw three vertices in a color-only dynamic rendering pass. The input and output must be distinct images. The pipeline does not own render targets, descriptor sets or frame scheduling.

Exposure is in stops (default 0, clamped to -16 through 16): +1 doubles incoming radiance, -1 halves it. The pass uses the [Narkowicz ACES filmic approximation](https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/) after exposure, clamps negative RGB to zero, and preserves alpha. This is a fitted display curve rather than a full ACES color-management transform. Output remains display-linear; use an sRGB attachment for automatic encoding, or perform the encoding later if rendering into a linear intermediate. Do not apply an extra gamma operation on top of an sRGB attachment.

Run this pass after HDR lighting, alpha blending and sample averaging. Synchronize color-attachment writes (rasterization) or ray-tracing storage writes against fragment sampled reads before the pass. The test viewer uses the same tone mapper for rasterized and ray-traced scenes, with or without IBL. `--exposure -1` lowers exposure by one stop. Wireframe mode retains its diagnostic white/blue output.

## IOR and transmission

Materials expose `ior` (default 1.5), `transmission_factor` (default zero), and `transmission_texture`. Transmission samples the linear red channel. It replaces the dielectric diffuse contribution, preserves specular reflection, multiplies transmitted radiance by base color, and has no effect on fully metallic materials. Alpha still controls coverage. IOR changes dielectric F0 in direct lighting, ambient lighting, and IBL. IOR 1 removes dielectric reflection; the glTF special value 0 represents infinite IOR and blocks transmission.

Volume materials use geometric entry/exit surfaces and Snell's law, with reflected continuation for total internal reflection. Zero thickness selects thin-walled transmission, without macroscopic bending. Roughness samples a GGX microfacet normal; this remains a bounded preview approximation rather than an unbiased BTDF integrator. The medium stack tracks nested closed instances and their IORs. Surface tint/transmission is applied at entry; volume absorption acts on the intervening world-space distance. Exiting restores the enclosing medium, and internal reflection retains it.

Ray generation launches one primary ray and at most `min(sampling.w, 8)` continuation rays. Closest-hit returns the next origin, direction and throughput; ray generation performs the next trace. Pipeline recursion depth remains 1. Reaching the limit discards the unresolved continuation. This keeps traversal bounded even with internal reflection. Advance the seed and accumulate externally to converge rough transmission noise. The raster viewer uses one transmission sample per pixel; ray tracing averages the configured AA samples.

For raster refraction, enable buffer device addresses, acceleration structures, descriptor nonuniform indexing and `VkPhysicalDeviceRayQueryFeaturesKHR::rayQuery`, including their required device extensions. Supply `pipeline_create_info.ray_query_layout` and `texture_count`. The query set follows the existing sets: set 2 without IBL, set 3 with IBL. Create it with `ray_query_bindings(texture_count)`:

| Binding | Descriptor | Resource |
| --- | --- | --- |
| 0 | Uniform buffer | `ray_tracing_scene_uniform`, also used for query limits, mask, background and texture LOD |
| 1–4 | Storage buffers | Vertices, indices, instances and materials, as for the ray pipeline |
| 5 | Acceleration structure | The same mesh TLAS |
| 6 | Combined image sampler array | Fallback plus scene textures |

The ordinary graphics scene and material sets remain at 0 and 1. Graphics set 0 adds the light SSBO at binding 2. The material set has 17 samplers (see the slot table below). `material_uniform` is 992 bytes; `optical` contains IOR, transmission, thickness and dispersion, and `attenuation` contains RGB attenuation color and attenuation distance. Synchronize BVH builds and resource uploads with fragment shader reads. Refresh query instance/material buffers alongside graphics material descriptors. Leaving the query layout empty keeps the graphics shader free of ray-query device requirements; transmitted radiance then needs a different client solution.

The test viewer enables queries and builds the BVH automatically for raster scenes containing transmitting materials. Both query traversal and ray any-hit respect masked and blended coverage of secondary hits.

References: [KHR_materials_ior](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_ior), [KHR_materials_transmission](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_transmission).

## Volume and GlassVaseFlowers

`KHR_materials_volume` imports `thickness_factor` (default 0), `thickness_texture`, `attenuation_color` (default white), and `attenuation_distance` (default positive infinity). Both pipelines trace geometry, so they use actual world-space travel distance and ignore the baked thickness texture for transport, as specified for ray tracers. The texture is still preserved as a linear KTX resource for other clients. Only a positive thickness factor switches on a volume boundary; `double_sided` does not cull that boundary. Geometry must form a closed, consistently oriented surface for each volume instance.

Transmittance over distance `d` is `pow(attenuation_color, d / attenuation_distance)`. Infinite attenuation distance is lossless; zero color components absorb completely over a positive distance. Instance scaling affects the traced path length, while attenuation distance remains in world units. Absorption applies to every segment in the current medium, including segments ending on an opaque object inside it. The stack restores outer media when exiting nested instances; overlapping media use the most recently entered instance. Primary rays begin in air, with an isolated inside-volume fallback when their first boundary is a back face. Arbitrary camera placement inside nested media is not preclassified.

For graphics clients, draw with `firstInstance` equal to the index in the query instance buffer. The vertex shader passes `gl_InstanceIndex` to fragment shading so the primary raster hit and later query hits identify the same volume. Continue to update BVH flags when thickness changes between zero and positive values. A volume split into separate primitive instances should be assembled into one closed boundary before rendering with this stack.

`vulkanizer_glass_vase_flowers_test` uses the same graphics/ray-traced switch, optional IBL, tone mapping and orbit camera as IORTestGrid. Run normally for graphics ray queries, or with `--ray-traced --samples 8`. Converted assets and attribution are in `test/mesh/resources/glass_vase_flowers`. The source uses default lossless attenuation, so colored absorption and scale dependence are checked by the closed-slab GPU regression. Both builds register graphics and ray-traced scene tests.

The maximum remains eight secondary bounces. Unresolved paths at that limit contribute no further radiance. Scattering, diffuse GI, and separately traced non-TIR reflection branches are outside this reference pipeline.

Reference: [KHR_materials_volume](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_volume).

The IORTestGrid and GlassVaseFlowers viewers use `studio_small_09_2k.hdr`. Add `--blur` to display the irradiance cubemap as the background in either mode. Irradiance is divided by pi for display so constant environments retain their brightness. Lighting and secondary rays retain their normal environment lookups. `--no-ibl` disables the background, including blur.

The default environment has been restored to `studio_small_09_2k.hdr`. `--blur` remains available. Raster `--samples N` enables MSAA with 1, 2, 4, 8, 16, 32 or 64 samples; unsupported counts fall back to the highest supported count below the request and are reported at startup. Without this option rasterization uses one sample. Ray tracing retains its 1-32 sample range and default of 8. MSAA resolves the HDR color before tone mapping; wireframe resolves directly to the swapchain.


## Material extensions

Both pipelines support `KHR_texture_transform`, `KHR_materials_specular`, `KHR_lights_punctual`, `KHR_materials_emissive_strength`, `KHR_materials_unlit`, `KHR_materials_clearcoat`, `KHR_materials_sheen`, `KHR_materials_anisotropy`, `KHR_materials_iridescence`, and `KHR_materials_dispersion` in addition to IOR, transmission and volume.

`make_material_uniform` fills factors and per-slot transforms. UV evaluation follows the Khronos sample renderer's top-left texture convention: `u = offset.x + cos(r)*scale.x*u + sin(r)*scale.y*v`, `v = offset.y - sin(r)*scale.x*u + cos(r)*scale.y*v`, using the original UV components after selecting the texture's resolved UV set. Transformed normal maps derive their frame from transformed UVs. Clearcoat normal maps have their own UV frame and scale. Unlit uses base color, vertex color and alpha coverage, independent of lighting and emission.

| Slot | Graphics binding | Texture | Encoding/channel |
| --- | --- | --- | --- |
| 0 | 1 | Base color | sRGB RGB, linear alpha |
| 1 | 2 | Metallic/roughness | Linear B/G |
| 2 | 3 | Normal | Linear RGB |
| 3 | 4 | Occlusion | Linear R |
| 4 | 5 | Emissive | sRGB RGB |
| 5 | 6 | Transmission | Linear R |
| 6 | 7 | Thickness | Linear G, retained for clients |
| 7 | 8 | Specular weight | Linear A |
| 8 | 9 | Specular color | sRGB RGB |
| 9 | 10 | Clearcoat | Linear R |
| 10 | 11 | Clearcoat roughness | Linear G |
| 11 | 12 | Clearcoat normal | Linear RGB |
| 12 | 13 | Sheen color | sRGB RGB |
| 13 | 14 | Sheen roughness | Linear A |
| 14 | 15 | Anisotropy | Linear RG direction, B strength |
| 15 | 16 | Iridescence | Linear R |
| 16 | 17 | Iridescence thickness | Linear G |

Specular adjusts dielectric F0/F90 and diffuse energy without changing metals or the refraction angle. Emissive strength is folded into the uploaded linear emissive factor. Clearcoat adds an independent dielectric GGX lobe; sheen uses Charlie distribution/visibility and albedo scaling. Anisotropy rotates the tangent-space direction and stretches GGX roughness along it. Iridescence evaluates thin-film interference with the Khronos sample renderer's spectral sensitivity approximation. Its adapted implementation is covered by `LICENSES/glTF-Sample-Renderer.txt`.

IBL retains GGX split-sum filtering, with a bent-normal approximation for anisotropy and an approximate thin-film Fresnel integration. Sheen evaluates a 32-direction hemisphere quadrature against the environment and uses the same quadrature for its energy correction; this is a reference-quality cost/accuracy tradeoff, not a prefiltered Charlie cubemap. Without IBL, the ambient term remains a diffuse approximation; extension lobes are evaluated for explicit lights.

Upload `make_light_uniforms(scene)` to a storage buffer and bind it at graphics set 0 binding 2 or ray set 0 binding 7. The helper returns one neutral element for an empty scene so a valid buffer can always be bound. Set `scene_uniform.vertex_layout.y` to the actual light count. Light records store position/range, direction/type, linear color times intensity, and cosines of the inner/outer spot angles. Types are directional=0, point=1, spot=2. Point/spot lights use inverse-square distance attenuation and the glTF range/spot falloffs. They are unshadowed. The legacy directional fields remain additive; set their radiance to zero when only imported lights should contribute. The sample viewer does this automatically.

Set `scene_uniform.vertex_layout.z` to 1 when any scene material uses volume dispersion, otherwise 0. Both modes then trace independent red, green and blue paths, with per-channel IOR spread `(ior - 1) * 0.025 * dispersion`, retaining the selected channel through nested media. This triples tracing work; AA still averages the requested number of pixel samples. Each channel retains the eight-secondary-bounce limit. Dispersion has no effect on thin surfaces without volume. The existing refraction/volume limitations still apply.

The expanded material and light bindings change the shader ABI. Rebuild clients and recreate their layouts/buffers from the public declarations. Material caches remain backward compatible; reconvert source assets to obtain extension values omitted by older converters.

## Extension sample viewer

`vulkanizer_material_extensions_test --scene NAME` selects one of the converted scenes in `test/mesh/resources/extensions`. Add `--ray-traced`, `--samples`, `--blur`, `--no-ibl`, `--exposure`, or `--screenshot` as with the other viewers. The default is `TextureTransformTest`; all scenes use the orbit camera and studio environment.

The scenes follow implementation order: `TextureTransformTest`, `SpecularTest`, `LightsPunctualLamp`, `EmissiveStrengthTest`, `UnlitTest`, `ClearCoatTest`, `SheenTestGrid`, `AnisotropyStrengthTest`, `IridescenceDielectricSpheres`, and `DispersionTest`. Both rendering modes are registered in CTest. Attribution and original license metadata accompany each converted model.


SBT storage uses `VMA_MEMORY_USAGE_GPU_ONLY` and `TRANSFER_DST`. `vkz::trace_ray`
records its staging copy and transfer-to-ray-tracing barrier on first use. That first
command buffer must be submitted before later traces using the pipeline. Clients
issuing raw trace commands must call `pipeline.sbt.upload(command_buffer)` first.
The immutable staging allocation stays alive with the SBT. BVH instance build input
also uses GPU-only storage; its staging copy is recorded before the build/update.
Finish previous uses before updating instances or destroying these resources.
