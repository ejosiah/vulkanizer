# Raster GI test

`rtx_test` renders the scene at 1440 � 1280, with GI, shadows and reflections at half resolution.

The Lighting panel selects **DDGI** or **NRC**. NRC uses the NVIDIA NRC SDK bridge from the sibling
`light_transport` project. Both query and training paths reconstruct their initial position and
normal from the raster G-buffer and start their first indirect ray there. Training traces up to
four secondary surfaces. The cache predicts radiance at the first secondary hit; the forward
material shader applies the receiving surface color separately. The cache learns online and
resets when scene lights change. The GI multiplier and indirect-only view apply to either method.

**NRD denoising** selects RELAX diffuse/specular filtering for the indirect and reflection signals,
and a separate SIGMA shadow instance for each scene light. SIGMA receives world-space penumbra
radii and its output is decoded back to light visibility. RELAX receives view depth, encoded
world normals/roughness, camera matrices, static world-space motion vectors and ray hit distances.
When NRD is enabled, both the built-in shadow temporal/spatial filter and reflection SVGF passes
are skipped. Disabling NRD restores those filters. Histories restart on changes to the GI mode,
light enable states, reflection enable state, or NRD enable state. All descriptor sets are ordinary
private sets; the raster GI ray pipeline borrows the reflection pass's geometry descriptors.

CMake reuses the existing SDK installations/builds from `../light_transport`. Independent
installations can be specified through `VULKANIZER_NRC_SDK_ROOT`, `VULKANIZER_NRD_SDK_ROOT`,
`VULKANIZER_NRD_LIBRARY` and `VULKANIZER_NRD_BLOB_LIBRARY`. The NRD library must embed SPIR-V,
use normal encoding 2 and roughness encoding 1, and disable quad intrinsics. NRC and its CUDA
runtime DLLs are copied beside the test executable. SDK licenses remain in their installations.

Diagnostic runs:

```
rtx_test --nrc --nrd --timeout 10 --validation --verify-lighting --no-game-controller
rtx_test --nrd --cycle-denoising --timeout 10 --validation --verify-lighting --no-game-controller
rtx_test --living-room --nrc --nrd --timeout 10 --validation --verify-lighting --no-game-controller
```

`--verify-lighting` reads back the GPU outputs, checks finite/nonnegative radiance and verifies
that SIGMA visibility is finite, in [0, 1], and preserves variation when its raw input has both lit and shadowed pixels.
`--cycle-denoising` exercises NRC/DDGI and NRD on/off transitions. GPU timings include NRC and NRD,
and collect only queries actually recorded in the previous frame.

The local NRC 0.15 SDK reports live SDK/CUDA NVX objects at Vulkan device destruction after its
`Context::Destroy` and `Shutdown` calls, also seen in the reference's cleanup logs. These are
shutdown validation reports; record-time synchronization and descriptor errors are checked separately.

Separate full-resolution checkboxes switch GI, shadows, and reflections with their denoisers between
720x640 and 1440x1280. It preserves the scene, camera, and light settings, recreates lighting
resources, and resets temporal histories. `--full-resolution` selects it at startup;
`--full-resolution-gi`, `--full-resolution-shadows`, and `--full-resolution-reflections` select individual effects.
`--cycle-resolution` exercises mixed resolutions.

Probe offsetting is optional and currently enabled by default for the density comparison. The toggle rebuilds probe resources,
so disabling it restores the original grid. The chapter 15 backface ratio (25%), frontface
clearance (5 cm), maximum frontface move (20 cm), and cell limits guide relocation;
movement follows geometric normals as requested. `--probe-offsetting` enables it at startup.

### Alpha in the living room lamps

The authored lamp shell uses glTF alpha blend with partial coverage. Both demos retain the authored material alpha and texture alpha. The ceiling point light uses its original source size, and the lamp has no emission override.

For transparent materials, shadow ray queries
sample base-color alpha (including vertex alpha and UV transforms): MASK rejects
holes, and BLEND attenuates visibility by one minus alpha. The NRD/SIGMA path
turns that fractional visibility into stochastic binary penumbra samples before
denoising. DDGI, reflection and NRC bounce rays use stochastic blend coverage.

Forward blended surfaces trace their own world-space direct shadows. They do
not consume opaque G-buffer GI/reflection signals, which belong to the surface
behind the shell and would leave visible temporal trails. Transparent GI and
reflection shading needs a separate surface-aware path; it is not taken from
the opaque screen-space textures.

Use `--gi-multiplier <value>` to set the initial GI strength in rtx_test and
the VulkanXR Sponza/living room examples. The default is 1; 0 disables the GI
contribution, and larger values exaggerate it. Values must be finite and non-negative.

### Living room TV

The TV plays `test/resources/video/3116737-hd_1920_1080_25fps.mp4` (the cat clip)
in a continuous loop using Vulkan H.264 decoding. The screen uses a persistent
linear RGB texture so rasterization, DDGI probe rays, reflections and NRC all
see the same current frame. DDGI can now borrow the renderer's material textures
instead of uploading a second independent copy.

Direct TV lighting uses textured Linearly Transformed Cosines (LTC), following
[the original authors' publication and textured-emitter demo](https://eheitzresearch.wordpress.com/415-2/)
and [their official 64�64 lookup tables and polygon integration](https://github.com/selfshadow/ltc_code).
Diffuse and GGX specular contributions evaluate the filtered emitter separately
in their transformed spaces. Each new video frame generates a padded cosine
filter hierarchy on the GPU; its levels use the original demo's factor-of-three
footprint spacing. Visibility uses the existing alpha-aware scene ray queries
and follows the scene shadow toggle. The current visibility approximation traces
to the screen center; it does not integrate partially occluded emitter regions.
The screen emission and LTC light both use a radiance multiplier of 20.

`--verify-lighting` checks video/filter radiance and playback progress, and
compares scene color with TV LTC lighting enabled and disabled, holding the video
frame and indirect lighting fixed. LTC licensing and attribution are in
`LICENSES/ltc.txt`.
`--video-path "C:/path/to/video.mp4"` selects a different TV video. Omit it to use the cat clip. Supported files follow the Vulkan H.264 playback requirements in `docs/video.md`.

TV audio uses vkz::audio media decoding and its sample clock to synchronize video. Audio starts after the first rendered frame and restarts with video on each loop. The default cat clip contains a silent audio track; select a clip with audible audio using --video-path to hear playback.
