#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <filesystem>
#include <optional>
#include <memory>
#include <variant>
#include <vector>

namespace vkz::audio {
    inline constexpr uint32_t sample_rate = 48000;
    inline constexpr uint32_t channels = 2;
    using clock = std::chrono::steady_clock;

    struct pcm {
        // Interleaved stereo float samples at 48 kHz. Delay is relative to clip.start_time.
        std::vector<float> samples;
        double delay_seconds{};
    };

    // Decode the media audio track to the engine format. Empty means no audio track.
    // MP4/AAC decoding uses Media Foundation on Windows; other platforms report unsupported.
    [[nodiscard]] std::optional<pcm> load_media(const std::filesystem::path &path);

    struct clip {
        // A generator runs on the engine worker. Capture owned values, never live scene objects.
        std::variant<pcm, std::function<pcm()>> data;
        clock::time_point start_time{clock::now()};
    };

    struct params {
        uint32_t max_clips{16}; // Includes queued, generating, delayed and playing clips; maximum 64.
        bool null_device{};     // Silent device for automated tests or headless applications.
    };

    struct properties {
        float volume{0.8f}; // [0, 1]; zero mutes currently playing clips too.
    };

    struct statistics {
        uint64_t accepted{};
        uint64_t rejected{};
        uint64_t completed{};
        uint64_t failed{};
        uint32_t outstanding{};
    };

    class engine {
      public:
        explicit engine(const params &params = {});

        ~engine();

        engine(const engine &) = delete;

        engine &operator=(const engine &) = delete;

        // Nonblocking. False means full/busy: the caller retains the clip and may retry.
        bool submit(clip &&clip);

        void update(const properties &properties);

        [[nodiscard]] statistics stats() const;

        // Replace the independent media transport; returns with the PCM prepared and paused.
        // Complete tracks up to 300 seconds. submit() keeps Vista's bounded one-shot semantics.
        void load(pcm pcm);
        void pause(bool paused);
        void seek(double seconds);
        [[nodiscard]] double position() const;

      private:
        class impl;
        std::unique_ptr<impl> impl_;
    };
} // namespace vkz::audio
