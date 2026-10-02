# IORTestGrid

Converted from Khronos glTF-Sample-Assets, `Models/IORTestGrid/glTF/IORTestGrid.gltf`.
Source: https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/IORTestGrid
Author: Eric Chadwick. License: CC0 1.0; see LICENSE.md.

The scene contains 23 meshes and 23 instances. Images are converted to KTX and embedded in IORTestGrid.vkmesh, with materials in IORTestGrid.vkmat and bounds in IORTestGrid.vkbounds.

The cache imports KHR_materials_ior and KHR_materials_transmission. Graphics uses ray queries for transmitting surfaces; ray tracing launches secondary rays, capped at eight bounces. The cache now includes KHR_materials_volume and its absorption parameters. Both paths use actual geometric thickness. The cache was regenerated with material version 5 and now includes KHR_materials_specular.

Run vulkanizer_ior_test_grid_test for graphics, or add --ray-traced for ray tracing. The ray-traced mode defaults to 8 AA samples; --samples accepts 1 through 32. Hold the left mouse button to orbit and use the wheel to zoom. --screenshot output.png and --timeout 3 are also available.

The environment asset `studio_small_09_2k.hdr` was copied from the shared `vglib_examples/data/textures/environment` folder used by gltf_viewer. Both rendering modes use it by default for the background and image-based lighting, alongside the reference directional light. Startup generates a 512-pixel environment cubemap, 32-pixel irradiance cubemap and 256-pixel GGX specular cubemap with nine roughness mips. Add `--no-ibl` to restore the directional-light/ambient baseline. The model license above applies to IORTestGrid, not to this separate environment asset.

Both rendering modes tone-map the HDR scene before sRGB output. `--exposure` accepts stops from -16 to 16 (default 0); for example, `--exposure -1` halves the incoming radiance before the filmic curve.


The default environment has been restored to `studio_small_09_2k.hdr`. `--blur` remains available. Raster `--samples N` enables MSAA with 1, 2, 4, 8, 16, 32 or 64 samples; unsupported counts fall back to the highest supported count below the request and are reported at startup. Without this option rasterization uses one sample. Ray tracing retains its 1-32 sample range and default of 8. MSAA resolves the HDR color before tone mapping; wireframe resolves directly to the swapchain.
