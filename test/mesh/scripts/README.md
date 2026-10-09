# Sample model scripts

Run these commands from the Vulkanizer repository root with Python 3.10 or newer. They use only the Python standard library. Build `vulkanizer_mesh_convert_test` and `vulkanizer_material_extensions_test` first. Both scripts default to the Release build; pass `--config Debug` to use the Debug build.

## Convert a local file

```powershell
python test/mesh/scripts/convert_model.py "C:/models/Example.gltf" "C:/converted models"
python test/mesh/scripts/convert_model.py "C:/models/Example.glb" "C:/converted models" --config Debug
python test/mesh/scripts/convert_model.py test/resources/bistro/Exterior/exterior.obj test/mesh/resources/bistro --scale 0.01
python test/mesh/scripts/convert_model.py test/resources/bistro/Interior/interior.obj test/mesh/resources/bistro --scale 0.01
```

The output argument is a directory, created if needed. The source filename supplies the name: `Example.gltf` produces `Example.vkmesh`, `Example.vkmat`, and `Example.vkbounds` directly in that directory. Textures are embedded as KTX2. Existing files with those names are replaced after successful conversion. Source files and their referenced textures/buffers must already be available locally; this script does not download them.

Use `--scale` to bake a positive uniform unit conversion into the output; Bistro uses `--scale 0.01` to convert centimetres to metres.

It uses the same `--config`, `--build-dir`, `--converter`, and `--ktx` options as the sync script. Conversion errors propagate as a nonzero exit status.

## Convert missing models

```powershell
python test/mesh/scripts/sync_models.py --dry-run
python test/mesh/scripts/sync_models.py
python test/mesh/scripts/sync_models.py --models BoxTextured "Box With Spaces"
python test/mesh/scripts/sync_models.py --limit 5 --config Debug
```

The script checks the [Khronos model catalog](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models) at a pinned commit for each run. It skips names already present anywhere under `test/mesh/resources` with all three `.vkmesh`, `.vkmat`, and `.vkbounds` files. Existing complete models are not refreshed when upstream changes.

Missing models are downloaded and converted into `test/mesh/resources/models/<Model>/`. Textures remain embedded as KTX2 in the converted assets. Model attribution, license files, and conversion provenance are saved alongside them. Source downloads are cached under `build/sample-assets/<commit>/`; converter logs and `last_sync.json` are under `build/sample-assets/`.

The converter's existing format limitations apply. Unsupported models are reported as failures, and the script continues with remaining models. A run with any failures exits with status 1. Animation is not baked into the converted static scene.

To reuse an existing checkout's source files while checking the online catalog:

```powershell
python test/mesh/scripts/sync_models.py --models BoxTextured --source-dir "C:/path/to/glTF-Sample-Assets/Models"
```

Use `--resources-dir`, `--cache-dir`, or `--report` to change destinations. Use `--build-dir`, `--converter`, and `--ktx` for custom tool locations. KTX defaults to `VKZ_KTX_EXECUTABLE`, the repository's `build/tools/ktx/bin/ktx.exe`, or `ktx` on PATH. `--help` lists all options. A full catalog run may download substantial data.

## Render a model

```powershell
python test/mesh/scripts/render_model.py Sponza --ray-traced --samples 8
python test/mesh/scripts/render_model.py BoxTextured --samples 4 --blur
python test/mesh/scripts/render_model.py GlassVaseFlowers --ray-traced --samples 32 --exposure -1
python test/mesh/scripts/render_model.py "Box With Spaces" --config Debug --validation --timeout 2 --screenshot build/box.png
```

Supply a model name or a `.vkmesh` path. Names are matched case-insensitively across the resources tree; ambiguous names require an explicit path. The script launches the shared reference test with the selected mesh and the existing studio environment.

Supported options: `--ray-traced`, `--wireframe`, `--samples`, `--blur`, `--no-ibl`, `--exposure`, `--validation`, `--timeout`, `--screenshot`, and `--no-game-controller`. Raster MSAA uses `--samples`; ray tracing defaults to 8 samples and supports up to 32. `--camera orbit|fps` selects the camera, defaulting to FPS for Sponza and orbit otherwise.

Use `--config`, `--build-dir`, or `--executable` to select a viewer build. `--resources-dir` selects another model root. `--dry-run` prints the command without launching the viewer. Viewer errors propagate as the script's exit status.
