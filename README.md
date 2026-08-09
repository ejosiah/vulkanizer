# Vulkanizer

Vulkanizer is a small C++20 helper library for building Vulkan applications. It wraps common setup and rendering tasks while keeping the underlying Vulkan handles and commands accessible.

The library currently provides:

- Vulkan instance, device, surface, queue, and swapchain setup
- Owning and non-owning Vulkan contexts
- A reusable GLFW-based `vulkan_app`
- Spectator and first-person camera controllers
- A standalone GLFW input adaptor with combined keyboard, mouse, and gamepad input
- Graphics and compute pipeline builders
- In-process inline GLSL compilation through glslang
- Dynamic rendering and synchronization helpers
- Command pools, scoped command buffers, ring fences, and batched queue submission
- Vulkan Memory Allocator-backed buffers and images with explicit cleanup and non-owning allocator support
- Reusable staging-buffer memory with aligned borrowing, GPU transfer helpers, and explicit range returns
- Descriptor, primitive, transform, CSM, and Dear ImGui utilities

## Requirements

- CMake 3.24 or newer
- A C++20 compiler
- Vulkan SDK, including `glslc` when building the tests
- Conan 2

The Conan recipe supplies Volk, GLFW, Dear ImGui, GLM, and Vulkan Memory Allocator.

## Build

Install the dependencies and generate the Conan toolchain:

```powershell
conan install . --output-folder=build --build=missing -s build_type=Release
```

Configure and build with the generated preset:

```powershell
cmake --preset conan-default
cmake --build --preset conan-release
```

Tests are enabled by default. Disable them for a library-only build:

```powershell
cmake --preset conan-default -DVULKANIZER_BUILD_TESTS=OFF
```

To create the Conan package locally:

```powershell
conan create . --build=missing
```

This repository currently produces `vulkanizer/2.5.0`.

## Use from CMake

After making the package available to Conan, add it to your consuming recipe and link the exported target:

```cmake
find_package(vulkanizer CONFIG REQUIRED)

target_link_libraries(my_app PRIVATE vulkanizer::vulkanizer)
target_compile_features(my_app PRIVATE cxx_std_20)
```

A minimal application can use the library-owned GLFW/Vulkan bootstrap:

```cpp
#include <vulkanizer/vulkan_app.hpp>

int main() {
    vkz::vulkan_app app{{
        .width = 1280,
        .height = 720,
        .title = "My Vulkan app",
    }};

    auto swapchain = app.create_swapchain();

    while (!app.should_close()) {
        app.poll_events();
        // Acquire, render, and present.
    }
}
```

## Camera input

The GLFW input adaptor is provided by its own header. Create it for the application window, bind it, and pass its source-agnostic input device to a camera controller:

```cpp
#include <vulkanizer/glfw_input_adaptor.hpp>

vkz::glfw_input_adaptor input(app.window());
input.bind();

vkz::camera::camera camera;
vkz::camera::controller controller{
    camera,
    vkz::camera::movement_type::spectator,
    input.get_device(),
};

// Once per frame:
input.process_game_pad_input();
controller.process_input();
controller.update(delta_seconds);
```

Keyboard and gamepad state are merged, so either source can remain active without an idle source cancelling it. When multiple sources hold the same direction, the strongest input speed is used.

Default controls are:

| Input | Action |
|---|---|
| `W` / `S` | Forward / backward |
| `A` / `D` | Left / right |
| `Q` / `E` | Down / up |
| Left mouse drag | Look around |
| Mouse wheel | Zoom |
| D-pad or left stick | Move horizontally |
| Left / right trigger | Down / up |
| Right stick | Look around |

Construct the controller with `movement_type::spectator` for free-flight movement or `movement_type::first_person` for movement constrained relative to world up.

## Examples

The executables under `test/` are interactive examples:

- `vulkanizer_context_test` - application, swapchain, and Dear ImGui setup
- `vulkanizer_primitive_test` - generated primitive rendering
- `vulkanizer_csm_test` - cascaded shadow mapping
- `vulkanizer_camera_test` - camera input, movement switching, cubemap skybox, and an infinite checkerboard floor

On a Visual Studio Debug build, the camera example can be run with:

```powershell
.\cmake-build-debug\test\Debug\vulkanizer_camera_test.exe
```

## License

MIT
