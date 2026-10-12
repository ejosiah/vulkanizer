#pragma once
#include <vulkanizer/audio.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace vkz::audio::detail {
    inline constexpr size_t voice_capacity = 64;

    // One producer and one consumer. Ownership crosses only after release/acquire publication.
    template <typename T, size_t Capacity> class spsc_queue {
      public:
        bool push(T value) noexcept {
            const auto write = write_.load(std::memory_order_relaxed);
            const auto next = (write + 1) % (Capacity + 1);
            if (next == read_.load(std::memory_order_acquire)) {
                return false;
            }
            entries_[write] = value;
            write_.store(next, std::memory_order_release);
            return true;
        }

        bool pop(T &value) noexcept {
            const auto read = read_.load(std::memory_order_relaxed);
            if (read == write_.load(std::memory_order_acquire)) {
                return false;
            }
            value = entries_[read];
            read_.store((read + 1) % (Capacity + 1), std::memory_order_release);
            return true;
        }

      private:
        std::array<T, Capacity + 1> entries_{};
        alignas(64) std::atomic<size_t> read_{};
        alignas(64) std::atomic<size_t> write_{};
    };

    struct sound {
        std::vector<float> samples;
        int64_t start_frame{};
    };

    class mixer {
      public:
        // Replace only while the device callback is stopped.
        void load(pcm value) {
            media_ = std::move(value.samples);
            media_cursor_ = 0;
            media_position_.store(0);
            media_seek_.store(-1);
            paused_.store(true);
        }

        void pause(bool value) {
            paused_.store(value);
        }

        void seek(uint64_t frame) {
            media_seek_.store(static_cast<int64_t>(frame));
        }

        double position() const {
            const auto pending = media_seek_.load();
            return double(pending >= 0 ? pending : media_position_.load()) / sample_rate;
        }

        bool publish(sound *value) noexcept {
            return ready_.push(value);
        }

        bool reclaim(sound *&value) noexcept {
            return retired_.pop(value);
        }

        // Callback owns active_ and cursor_. No allocation, deletion, mutexes, logging or user code.
        void render(float *output, uint32_t frame_count, float volume) noexcept {
            std::fill_n(output, size_t(frame_count) * channels, 0.0f);
            const auto seek = media_seek_.exchange(-1);
            if (seek >= 0) {
                media_cursor_ = seek;
            }
            if (!paused_.load()) {
                const auto count = std::min<uint64_t>(frame_count, media_cursor_ < media_.size() / channels ? media_.size() / channels - media_cursor_ : 0);
                if (count) {
                    std::copy_n(media_.data() + media_cursor_ * channels, count * channels, output);
                }
                media_cursor_ += frame_count;
            }
            media_position_.store(media_cursor_);
            for (auto &slot : active_) {
                if (!slot) {
                    ready_.pop(slot);
                }
            }
            const auto end = cursor_ + frame_count;
            for (auto &slot : active_) {
                if (!slot) {
                    continue;
                }
                const auto clip_end = slot->start_frame + int64_t(slot->samples.size() / channels);
                const auto begin_frame = std::max(cursor_, slot->start_frame);
                const auto end_frame = std::min(end, clip_end);
                for (auto frame = begin_frame; frame < end_frame; ++frame) {
                    const auto source = size_t(frame - slot->start_frame) * channels;
                    const auto destination = size_t(frame - cursor_) * channels;
                    output[destination] += slot->samples[source];
                    output[destination + 1] += slot->samples[source + 1];
                }
                if (clip_end <= end && retired_.push(slot)) {
                    slot = nullptr;
                }
            }
            // Smooth master gain over 5 ms; limiting guarantees finite output in [-1,1].
            const auto step = 1.0f / (0.005f * sample_rate);
            for (uint32_t frame = 0; frame < frame_count; ++frame) {
                gain_ += std::clamp(volume - gain_, -step, step);
                for (uint32_t channel = 0; channel < channels; ++channel) {
                    auto &sample = output[size_t(frame) * channels + channel];
                    sample = std::clamp(sample * gain_, -1.0f, 1.0f);
                }
            }
            cursor_ = end;
        }

        // Only after the device has stopped and the callback can no longer run.
        void clear() {
            for (auto &slot : active_) {
                delete slot;
                slot = nullptr;
            }
            sound *value{};
            while (ready_.pop(value)) {
                delete value;
            }
            while (retired_.pop(value)) {
                delete value;
            }
        }

      private:
        spsc_queue<sound *, voice_capacity> ready_;
        spsc_queue<sound *, voice_capacity> retired_;
        std::array<sound *, voice_capacity> active_{};
        int64_t cursor_{};
        float gain_{};
        std::vector<float> media_;
        uint64_t media_cursor_{};
        std::atomic<int64_t> media_seek_{-1};
        std::atomic<int64_t> media_position_{};
        std::atomic<bool> paused_{true};
    };

    static_assert(std::atomic<size_t>::is_always_lock_free);
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<int64_t>::is_always_lock_free);
} // namespace vkz::audio::detail
