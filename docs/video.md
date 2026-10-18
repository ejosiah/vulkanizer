# Vulkan Video decode

[Video and audio class/sequence diagrams](media-integration.md).

`<vulkanizer/video.hpp>` provides CPU MP4 demux and Vulkan H.264 decode. The decoder
uses Vulkanizer buffers, images, views, samplers, descriptors, pipelines, barriers,
command pools and submissions. Video session/profile operations and YCbCr conversion
use Vulkan directly where the library has no equivalent.

The implementation follows the [Vulkan video coding specification](https://docs.vulkan.org/spec/latest/chapters/videocoding.html),
including profile-specific format queries, aligned bitstream ranges, session memory,
coding scopes, DPB slot binding and cross-queue synchronization. Decode output and DPB
may coincide or be separate, according to device capabilities. Converted RGB output is
retained independently of reference pictures so B-frame reordering cannot overwrite
a picture before presentation.

## Test player

Build `vulkanizer_video_test`. It uses the same three MP4 files as the
`vglib_examples/video_playback` example. Set `VULKANIZER_VIDEO_DIR` in CMake or pass
`--video-dir <directory>` at runtime. The default is `test/resources/video`.
The ImGui combo selects the clip; pause, restart and seek controls are available.
Playback stops at the last frame without looping.
Embedded audio plays through the [ported Vista audio engine](audio.md). Pause,
restart, seek and volume control the audio track as well.

Pass `--video-path "C:\path\video.mp4"` to `vulkanizer_video_test` to play a supplied
file and add it to the combo box. Repeat the argument to add more files; the last
one is selected initially. Available example clips remain in the list. Custom
playback works even when the original sample assets are absent. The supplied video
must use the supported H.264 format; its dimensions do not need to be 1920×1080.

```
vulkanizer_video_test --parser-only
vulkanizer_video_test --verify
vulkanizer_video_test --verify-playback --trace-dir <directory>
vulkanizer_video_test --verify --capture-dir <directory>
vulkanizer_video_test --video-path <file.mp4>
ctest -C Debug -R vulkanizer_video --output-on-failure
```

`--verify` presents every source frame once, checks decode/display ordering and
exercises restart, forward/backward seek and clip switching. It deliberately runs
as fast as decoding permits, rather than checking playback timing.
`--verify-playback` uses MP4 presentation timestamps and checks every source frame
was decoded and displayed without skipped or backward transitions. A hold longer than 1.5 times the
shortest source-frame duration fails this timing check. CSV traces record
every submitted render and source display index. Repeated render indices between
video presentation times are expected when rendering faster than the video rate.
For clips with audio, the trace also records `audio_seconds`; clock drift greater
than one source-frame duration fails verification. Verification uses a null audio
device; interactive playback uses the default output device.
`--capture-dir` saves first, middle and last decoded RGB frames for picture comparison.
Missing assets make CTest skip with exit code 77; validation errors fail GPU testing.

## Using the decoder

Before creating the device, request `video::required_extensions()`, a
`VK_QUEUE_VIDEO_DECODE_BIT_KHR` queue and `samplerYcbcrConversion`.
`vulkan_app_create_info::queues` supports requesting that additional queue.
Use `video::decode_queue_family()` and `get_queue()` to retrieve the H.264 queue.
Create a `video::clip` with `clip::load()`, then a `video::decoder` with the allocator
and graphics/decode queues and their family indices. The graphics queue must also
support compute and transfer operations. `clip::frame_at(seconds)` maps playback
time to display order; `decoder::frame(order)` returns a borrowed RGB texture.

Calls are synchronous and must run outside rendering on the queue-owning thread.
Complete draws using a returned texture before the next `frame()`/`reset()` call.
Backward seeking resets the session and decodes from the initial IDR picture.

Supported input is MP4 containing progressive 8-bit 4:2:0 H.264, POC type 0 with
equal top/bottom counts, short-term references, four-byte AVC NAL lengths and no
left/top cropping. Long-term reference marking,
other codecs, interlacing and stream profile/dimension changes are rejected.
The test clips contain 903, 1515 (cat), and 394 (squirrel) frames respectively.

Private parser sources retain their original notices in `src/video/third_party`.
The local H.264 parser correction respects per-slice reference-count overrides
when reading weighted prediction tables.
The MP4 demuxer accepts CTTS version 1 for signed composition offsets, including
downloaded clips whose B-frame presentation timestamps precede their decode timestamps.

## Local validation

On the RTX 4090 with Vulkan validation enabled, the complete 903-, 1515- and
394-frame clips passed decode-order, display-order, restart and seek checks with
no Vulkan validation errors. Real-time runs displayed all 2,812 source frames;
the audio-enabled run's maximum holds were 42.5 ms, 51.0 ms and 48.7 ms respectively,
below the 1.5-frame timing threshold. Maximum audio/video clock drift was 14.8 ms.
Nine first/middle/last captures were compared to OpenCV's software
decode (mean absolute RGB differences 1.6–4.6 out of 255, including conversion and
chroma-filter differences). Timing depends on the GPU, driver and system load.
