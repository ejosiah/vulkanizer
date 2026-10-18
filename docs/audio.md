# Audio

[Video and audio class/sequence diagrams](media-integration.md).

`<vulkanizer/audio.hpp>` ports Vista's worker/device engine and bounded mixer to
`vkz::audio`. It uses the same pinned miniaudio 0.11.23 dependency. PCM is interleaved
stereo float at 48 kHz. The device callback does no allocation, deletion, locking,
logging or user synthesis; generators and voice reclamation run on the worker.

`engine::submit()` retains the original owning one-shot/generator API, delayed/late
sample scheduling, 60-second clip limit, 16-voice default (64 maximum), statistics,
smoothed volume and peak limiting. A rejected submission retains caller ownership.
`params::null_device` enables unattended testing.
On Windows the null backend requests 1 ms timer resolution while its worker is
alive, then restores it. It also temporarily opts out of Windows 11's occlusion-based
timer throttling, restoring the process's previous policy when the final null device closes.

The independent media transport adds `load(pcm)`, `pause(bool)`, `seek(seconds)` and
`position()`. Loading replaces the old track with the callback stopped, then prepares
the new track paused at zero. Position follows the device callback's rendered PCM sample count,
including silent time after the audio ends. Pause freezes this clock. Seeking changes
the sample position in the callback without allocating. Media PCM is predecoded and
held in memory without a fixed duration limit; media transport does not consume
one-shot voice capacity. Stereo 48 kHz float PCM uses approximately 22 MiB per minute.
Stop callers before destroying the engine; synchronous media loads must not run from
the audio callback or a deferred generator.

`audio::load_media(path)` extracts an audio track using Windows Media Foundation's
Source Reader. MP4/AAC and other installed media codecs are decoded to float PCM,
then miniaudio resamples/downmixes to 48 kHz stereo. MP4 edit lists are applied per
track so AAC encoder priming and H.264 reorder delay do not shift the soundtrack.
Sample timestamps preserve gaps, initial delay and alignment with the normalized
video presentation timeline. Multiple/rate-adjusted media edits are rejected. No audio
track returns `std::nullopt`; malformed/unsupported media reports an exception.
The audio engine itself is portable; this media-track decoder currently requires
Windows. Miniaudio is used for playback, not as an MP4/AAC decoder.

The video test plays embedded audio automatically. Video selection follows
an interpolated clock locked to the audio sample count when a track is present,
and uses a steady clock otherwise. The player corrects phase over 100 ms and bounds
interpolation to half a video frame, avoiding jumps from callback block quantization.
Pause, volume, seek, restart, clip switching and the video endpoint control audio
as well. During a seek, audio remains paused until the sought video frame is submitted.
The clock does not measure additional hardware output latency.
`--verify` decodes pictures without audio; `--verify-playback` uses the null audio
device, checks clock drift stays within one video frame and traces both clocks.
`--silent` explicitly disables audio playback/extraction.

`vulkanizer_audio_test` checks Vista's queue wraparound/publication, delayed/late
mixing, limiting, reclamation, ownership on overflow, worker execution and shutdown.
Additional tests cover sample order, pause/seek/replacement, EOF silence and mono
44.1 kHz to stereo 48 kHz conversion. `--real-device` opens the default device;
`--media <path>` also checks decoded media PCM is finite and reports its energy.
Edit-list tests cover track offsets and leading empty edits. `--expect-onset <seconds>`
checks a decoded pulse's timing. Local validation passed all 12 selected regression
tests and all 2,812 original video frames with audio enabled. A pulse at 1 second
decoded at 1.0001 seconds. The first two original clips have silent audio tracks;
the squirrel clip contains audible audio.

References: [miniaudio device lifecycle](https://miniaud.io/docs/manual/),
[Media Foundation Source Reader](https://learn.microsoft.com/en-us/windows/win32/medfound/processing-media-data-with-the-source-reader).
