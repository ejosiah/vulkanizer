# Application frames in flight

`vkz::application` accepts a frames-in-flight count at construction. The default
is 1; zero throws `std::invalid_argument` before creating Vulkan resources.

```cpp
class demo : public vkz::application {
public:
    demo(vkz::vulkan_app_create_info info) : application(info, 3) {}
    void app_logic(VkCommandBuffer command_buffer) override {
        // Update resources[frame_index()] here, after its previous GPU use completes.
    }
};
```

The name/size constructor also accepts the count after its extension chain:
`application("Demo", {1280, 800}, {}, 3)`.

`frames_in_flight()` returns the configured count. `frame_index()` identifies the
current slot from zero to count minus one. The slot fence is waited before
`new_frame()` and `app_logic()`; the index advances after `end_frame()` for a
submitted frame. Failed acquisition does not advance it.

Allocate mutable GPU resources such as uniform buffers and descriptors per slot
when using multiple frames. The application cycles command pools, fences,
acquire semaphores and depth attachments itself. Presentation semaphores belong
to swapchain images so they are reused only after reacquiring the corresponding
image. Swapchain recreation waits for the device and rebuilds presentation
semaphores and depth attachments at the new extent.
