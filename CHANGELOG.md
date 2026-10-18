# Release notes

## 1.1.0 — 2026-10-13

### Vulkan enums and flags

- Added typed enums for buffer/image usage, image aspects, access masks, pipeline
  stages, shader stages and descriptor types, plus the remaining queue, creation,
  rendering, descriptor, swapchain, ray-tracing, query, diagnostic and video flags.
- Enabled bitmask composition through `enum_bit_flags`. Access masks and pipeline
  stages retain synchronization2's 64-bit values; legacy APIs reject masks that
  would truncate during conversion.
- Added named combinations, including `_read_write`, `_upload`, `_download` and
  `_transfer` presets. Upload adds transfer-destination usage, download adds
  transfer-source usage, and transfer adds both directions.
- Added 484 typed, inline `constexpr` aliases across 38 enum families, including
  `vkz::image_usage_sampled_upload`, `vkz::pipeline_stage_transfer` and
  `vkz::shader_stage_fragment`. Added flat type aliases such as `vkz::image_usage`
  and `vkz::pipeline_stage`; the scoped flag enums remain available.
- Migrated builders, helpers, library code and tests to the
  new types and flat constants, with explicit conversions at native Vulkan boundaries.
- Retained `descriptor<T>(...)` and descriptor-type inference from the C++ descriptor
  wrapper. Descriptor updates require no explicit Vulkan descriptor-type argument;
  the typed enum is available as top-level `vkz::descriptor_type`.

### BVH

- Moved the BVH to `vkz::bvh`, retaining `vkz::mesh::bvh` as a compatibility alias.
- Added the abstract `vkz::bvh::scene` interface with scene-owned build and instance
  options. Existing mesh build/update overloads delegate through `mesh::scene_adapter`,
  which resolves mesh layout, LOD selection, material inheritance and culling policy.
- Fixed instance upload reuse: each recorded update retains its own snapshot,
  including multiple updates before submission and separate submissions.
- Retained previous generations across rebuilds and move assignment. Destruction
  waits for submitted GPU work; `release_completed_resources()` reclaims retired
  generations and upload snapshots while preserving the active BVH.
- Added geometry content revisions and CPU mesh position/index hashing to detect
  changes requiring a rebuild. Incompatible instance updates throw in Debug and Release.
- Separated resource allocation, instance uploads and command recording into internal
  helpers, and documented cleanup, descriptor refresh and caller-owned resource lifetimes.

### Validation

- Verified alias type identity, bitmask composition, native-mask conversion and
  preservation of synchronization2 bits. Debug and Release package builds succeeded;
  flag migration tests matched the existing baseline with no new failures or Vulkan
  validation errors.
- Passed ray-tracing, shader binding table and DDGI tests with synchronization
  validation enabled, including small/large upload snapshots, separate submissions,
  rebuild/move before submission, cleanup, destruction and geometry content changes.

See [BVH documentation](docs/bvh.md) for the scene interface and lifetime rules.

## 1.0.1 — 2026-10-12

### Video and audio

- Added MP4/H.264 demux and Vulkan video decoding through `vkz::video`, including
  retained RGB outputs for B-frame presentation ordering.
- Added a miniaudio-based engine in `vkz::audio`, with embedded soundtrack
  decoding through Windows Media Foundation and pause, seek, restart and volume controls.
- Added `vulkanizer_video_test --video-path <file>` to play a supplied video and
  add it to the ImGui selection list. Repeat the argument to add multiple files;
  the last supplied file is selected initially. Playback stops without looping.
- Smoothed playback timing against the audio sample clock to avoid callback-block
  timing jumps, and aligned MP4 audio/video track edit lists.
- Fixed MP4 demux rejection of CTTS version 1, which supports signed composition
  offsets used by downloaded videos with B-frames.
- Removed the five-minute media-audio duration and seek limits. Complete audio
  tracks are still predecoded into memory; 48 kHz stereo float PCM uses about
  22 MiB per minute. One-shot audio retains its separate bounded semantics.
- Added frame tracking, audio transport and media-timeline tests, and a CTTS
  regression test covering versions 0, 1 and unsupported-version rejection.

### Validation

- Tracked all 2,812 frames across the three original example clips with no missing
  frames or backward transitions. Measured audio/video clock drift stayed below 15 ms.
- Confirmed the downloaded `U6uPLzcz1.mp4` parses all 31,661 frames, decodes its full
  17-minute soundtrack, and passes a short GPU playback check with audio enabled.
- Passed the three video and two audio regression checks after their respective fixes.

See [video documentation](docs/video.md), [audio documentation](docs/audio.md),
and [class/sequence diagrams](docs/media-integration.md) for APIs and current limits.
