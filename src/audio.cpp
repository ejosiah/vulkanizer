#include <vulkanizer/audio.hpp>
#include "audio_mixer.hpp"
#include "audio_decode.hpp"
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <array>
#include <condition_variable>
#include <future>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace vkz::audio {
    namespace {
        class null_device_timer {
          public:
            explicit null_device_timer(bool enabled) {
#ifdef _WIN32
                if (!enabled) {
                    return;
                }
                std::lock_guard lock{mutex_};
                requested_ = true;
                if (users_++ == 0) {
                    previous_.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
                    if (GetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &previous_, sizeof(previous_))) {
                        auto state = previous_;
                        state.ControlMask |= PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
                        state.StateMask &= ~PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
                        changed_ = SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state));
                    }
                }
                active_ = timeBeginPeriod(1) == TIMERR_NOERROR;
#endif
            }

            ~null_device_timer() {
#ifdef _WIN32
                if (!requested_) {
                    return;
                }
                std::lock_guard lock{mutex_};
                if (active_) {
                    timeEndPeriod(1);
                }
                if (--users_ == 0 && changed_) {
                    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &previous_, sizeof(previous_));
                    changed_ = false;
                }
#endif
            }

          private:
            bool active_{};
            bool requested_{};
#ifdef _WIN32
            static inline std::mutex mutex_;
            static inline uint32_t users_{};
            static inline PROCESS_POWER_THROTTLING_STATE previous_{};
            static inline bool changed_{};
#endif
        };
    } // namespace

    class engine::impl {
      public:
        explicit impl(const params &settings) : settings_{settings} {
            if (!settings.max_clips || settings.max_clips > detail::voice_capacity) {
                throw std::invalid_argument("Audio max_clips must be in [1, 64]");
            }
            std::promise<void> initialized;
            auto ready = initialized.get_future();
            worker_ = std::thread([this, result = std::move(initialized)]() mutable {
                run(std::move(result));
            });
            try {
                ready.get();
            } catch (...) {
                worker_.join();
                throw;
            }
        }

        ~impl() {
            {
                std::lock_guard lock{mutex_};
                stopping_ = true;
            }
            wake_.notify_one();
            worker_.join();
        }

        bool submit(clip &&value) {
            std::unique_lock lock{mutex_, std::try_to_lock};
            if (!lock.owns_lock() || stopping_ || outstanding_.load() >= settings_.max_clips) {
                ++rejected_;
                return false;
            }
            pending_[write_] = std::move(value);
            write_ = (write_ + 1) % pending_.size();
            ++pending_count_;
            ++outstanding_;
            ++accepted_;
            lock.unlock();
            wake_.notify_one();
            return true;
        }

        void update(const properties &settings) {
            if (!std::isfinite(settings.volume) || settings.volume < 0 || settings.volume > 1) {
                throw std::invalid_argument("Audio volume must be finite and in [0, 1]");
            }
            volume_.store(settings.volume, std::memory_order_relaxed);
        }

        statistics stats() const {
            return {accepted_.load(), rejected_.load(), completed_.load(), failed_.load(), outstanding_.load()};
        }

        void load(pcm value) {
            if (value.samples.size() % channels || value.samples.size() > size_t(sample_rate) * channels * 300 || value.delay_seconds != 0) {
                throw std::invalid_argument("Media PCM must be stereo 48 kHz, at most 300 seconds, with no initial delay");
            }
            for (auto &sample : value.samples) {
                if (!std::isfinite(sample)) {
                    throw std::invalid_argument("Nonfinite media sample");
                }
                sample = std::clamp(sample, -1.0f, 1.0f);
            }
            std::lock_guard control{control_mutex_};
            std::promise<void> ready;
            auto result = ready.get_future();
            {
                std::lock_guard lock{mutex_};
                media_request_ = std::move(value);
                media_ready_ = std::move(ready);
            }
            wake_.notify_one();
            result.get();
        }

        void pause(bool value) {
            mixer_.pause(value);
        }

        void seek(double seconds) {
            if (!std::isfinite(seconds) || seconds < 0 || seconds > 300) {
                throw std::invalid_argument("Audio seek position must be in [0, 300] seconds");
            }
            mixer_.seek(static_cast<uint64_t>(std::llround(seconds * sample_rate)));
        }

        double position() const {
            return mixer_.position();
        }

      private:
        static void callback(ma_device *device, void *output, const void *, ma_uint32 frames) noexcept {
            auto &self = *static_cast<impl *>(device->pUserData);
            self.mixer_.render(static_cast<float *>(output), frames, self.volume_.load(std::memory_order_relaxed));
        }

        void prepare(clip value) {
            try {
                auto samples = std::holds_alternative<pcm>(value.data) ? std::move(std::get<pcm>(value.data)) : std::get<std::function<pcm()>>(value.data)();
                constexpr size_t max_samples = size_t(sample_rate) * channels * 60;
                if (samples.samples.size() % channels || samples.samples.size() > max_samples || !std::isfinite(samples.delay_seconds) || samples.delay_seconds < 0 || samples.delay_seconds > 120) {
                    throw std::invalid_argument("Invalid audio clip format, duration or delay");
                }
                for (auto &sample : samples.samples) {
                    if (!std::isfinite(sample)) {
                        throw std::invalid_argument("Nonfinite audio sample");
                    }
                    sample = std::clamp(sample, -1.0f, 1.0f);
                }
                const auto seconds = std::chrono::duration<double>(value.start_time - epoch_).count() + samples.delay_seconds;
                if (!std::isfinite(seconds) || std::abs(seconds) > 86400 * 365.0) {
                    throw std::invalid_argument("Audio timestamp outside supported range");
                }
                auto voice = std::make_unique<detail::sound>();
                voice->samples = std::move(samples.samples);
                voice->start_frame = int64_t(std::llround(seconds * sample_rate));
                if (!mixer_.publish(voice.get())) {
                    throw std::runtime_error("Audio ready queue exhausted");
                }
                voice.release();
            } catch (...) {
                ++failed_;
                --outstanding_;
            }
        }

        void reclaim() {
            detail::sound *voice{};
            while (mixer_.reclaim(voice)) {
                delete voice;
                ++completed_;
                --outstanding_;
            }
        }

        void run(std::promise<void> initialized) {
            // The null backend polls with Sleep(10); Windows' default 15.6 ms tick
            // otherwise quantizes the audio master clock beyond a 30 fps frame budget.
            null_device_timer timer{settings_.null_device};
            ma_device device{};
            auto config = ma_device_config_init(ma_device_type_playback);
            config.playback.format = ma_format_f32;
            config.playback.channels = channels;
            config.sampleRate = sample_rate;
            config.periodSizeInFrames = 256;
            config.dataCallback = callback;
            config.pUserData = this;
            const ma_backend null_backend = ma_backend_null;
            const auto result = ma_device_init_ex(settings_.null_device ? &null_backend : nullptr, settings_.null_device ? 1 : 0, nullptr, &config, &device);
            if (result != MA_SUCCESS) {
                initialized.set_exception(std::make_exception_ptr(std::runtime_error(std::string("Audio device initialization failed: ") + ma_result_description(result))));
                return;
            }
            epoch_ = clock::now();
            const auto started = ma_device_start(&device);
            if (started != MA_SUCCESS) {
                ma_device_uninit(&device);
                initialized.set_exception(std::make_exception_ptr(std::runtime_error(std::string("Audio device start failed: ") + ma_result_description(started))));
                return;
            }
            initialized.set_value();
            for (;;) {
                reclaim();
                std::optional<clip> next;
                std::optional<pcm> media;
                std::promise<void> media_ready;
                {
                    std::unique_lock lock{mutex_};
                    wake_.wait_for(lock, std::chrono::milliseconds(2), [this] {
                        return stopping_ || pending_count_ != 0 || media_request_.has_value();
                    });
                    if (stopping_) {
                        break;
                    }
                    if (pending_count_) {
                        next = std::move(pending_[read_]);
                        pending_[read_].reset();
                        read_ = (read_ + 1) % pending_.size();
                        --pending_count_;
                    }
                    if (media_request_) {
                        media = std::move(media_request_);
                        media_request_.reset();
                        media_ready = std::move(media_ready_);
                    }
                }
                if (media) {
                    const auto stopped = ma_device_stop(&device);
                    if (stopped == MA_SUCCESS) {
                        mixer_.load(std::move(*media));
                        const auto resumed = ma_device_start(&device);
                        if (resumed == MA_SUCCESS) {
                            media_ready.set_value();
                        } else {
                            media_ready.set_exception(std::make_exception_ptr(std::runtime_error("Audio device restart failed")));
                        }
                    } else {
                        media_ready.set_exception(std::make_exception_ptr(std::runtime_error("Audio device stop failed")));
                    }
                }
                if (next) {
                    prepare(std::move(*next));
                }
            }
            ma_device_uninit(&device); // Joins the callback before any clip memory is reclaimed.
            mixer_.clear();
            for (auto &entry : pending_) {
                entry.reset();
            }
        }

        params settings_;
        detail::mixer mixer_;
        clock::time_point epoch_;
        std::array<std::optional<clip>, detail::voice_capacity> pending_;
        std::mutex mutex_;
        std::condition_variable wake_;
        size_t read_{};
        size_t write_{};
        size_t pending_count_{};
        bool stopping_{};
        std::atomic<float> volume_{0.8f};
        std::atomic<uint64_t> accepted_{};
        std::atomic<uint64_t> rejected_{};
        std::atomic<uint64_t> completed_{};
        std::atomic<uint64_t> failed_{};
        std::atomic<uint32_t> outstanding_{};
        std::thread worker_;
        std::mutex control_mutex_;
        std::optional<pcm> media_request_;
        std::promise<void> media_ready_;
    };

    engine::engine(const params &settings) : impl_{std::make_unique<impl>(settings)} {
    }

    engine::~engine() = default;

    bool engine::submit(clip &&value) {
        return impl_->submit(std::move(value));
    }

    void engine::update(const properties &settings) {
        impl_->update(settings);
    }

    statistics engine::stats() const {
        return impl_->stats();
    }

    void engine::load(pcm value) {
        impl_->load(std::move(value));
    }

    void engine::pause(bool value) {
        impl_->pause(value);
    }

    void engine::seek(double seconds) {
        impl_->seek(seconds);
    }

    double engine::position() const {
        return impl_->position();
    }

    pcm detail::convert_pcm(std::span<const float> input, uint32_t input_channels, uint32_t input_rate) {
        if (!input_channels || !input_rate || input.size() % input_channels) {
            throw std::invalid_argument("Invalid decoded audio format");
        }
        const auto input_frames = input.size() / input_channels;
        const auto output_frames = ma_calculate_frame_count_after_resampling(sample_rate, input_rate, input_frames);
        pcm result;
        result.samples.resize(output_frames * channels);
        const auto converted = ma_convert_frames(result.samples.data(), output_frames, ma_format_f32, channels, sample_rate, input.data(), input_frames, ma_format_f32, input_channels, input_rate);
        if (!converted && input_frames) {
            throw std::runtime_error("Audio resampling failed");
        }
        result.samples.resize(converted * channels);
        return result;
    }
} // namespace vkz::audio
