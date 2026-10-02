# GlassVaseFlowers

Converted from the local Khronos glTF-Sample-Assets `Models/GlassVaseFlowers/glTF/GlassVaseFlowers.gltf`.
[Source asset](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/GlassVaseFlowers).
Glass vase by Eric Chadwick; flowers by Rico Cilliers. Model assets are CC0; see LICENSE.md for the accompanying metadata license.

The scene contains three shared meshes and four instances. The left vase uses alpha blending; the right vase uses transmission and volume. The source volume has thickness factor 0.075 and a thickness texture, with default white attenuation and infinite attenuation distance. It demonstrates geometric refraction without colored absorption. Analytic colored absorption is covered separately by the GPU slab regression.

Textures are converted to KTX2 and embedded in GlassVaseFlowers.vkmesh. Material version 4 data is in GlassVaseFlowers.vkmat; bounds are in GlassVaseFlowers.vkbounds.

Run `vulkanizer_glass_vase_flowers_test` for the graphics/ray-query pipeline, or add `--ray-traced --samples 8` for the ray-traced pipeline. Both use an orbit camera, the shared IORTestGrid environment map, and tone mapping. Hold the left mouse button to orbit; use the wheel to zoom. `--no-ibl`, `--exposure`, `--validation`, `--timeout`, and `--screenshot` are available as in the other reference viewers.

Both modes measure volume paths from geometry and ignore baked thickness values for absorption, retaining thicknessFactor as the thin/solid switch. The existing eight-secondary-bounce cap remains; trapped internal paths can become dark at the limit. The alpha vase uses stochastic coverage in ray tracing, so it converges with more samples.


The default environment has been restored to `studio_small_09_2k.hdr`. `--blur` remains available. Raster `--samples N` enables MSAA with 1, 2, 4, 8, 16, 32 or 64 samples; unsupported counts fall back to the highest supported count below the request and are reported at startup. Without this option rasterization uses one sample. Ray tracing retains its 1-32 sample range and default of 8. MSAA resolves the HDR color before tone mapping; wireframe resolves directly to the swapchain.
