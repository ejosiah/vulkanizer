# Release notes

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
