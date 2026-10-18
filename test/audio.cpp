#include <vulkanizer/audio.hpp>
#include "audio_mixer.hpp"
#include "audio_decode.hpp"
#include "audio_timeline.hpp"
#include <sstream>
#include <array>
#include <atomic>
#include <cmath>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
    using namespace vkz::audio;

    void require(bool condition, const char *message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    template <typename Predicate> void await(Predicate predicate) {
        const auto deadline = clock::now() + std::chrono::seconds(10);
        while (!predicate()) {
            require(clock::now() < deadline, "Audio test timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void test_queue() {
        detail::spsc_queue<uint32_t, 8> queue;
        for (uint32_t i = 0; i < 8; ++i) {
            require(queue.push(i), "Queue fills too early");
        }
        require(!queue.push(8), "Queue overflow accepted");
        uint32_t value{};
        for (uint32_t i = 0; i < 8; ++i) {
            require(queue.pop(value) && value == i, "FIFO order broken");
        }
        require(!queue.pop(value), "Empty queue returned data");
        std::atomic<bool> failed{};
        std::thread consumer([&] {
            for (uint32_t expected = 0; expected < 100000; ++expected) {
                uint32_t result{};
                while (!queue.pop(result)) {
                    std::this_thread::yield();
                }
                if (result != expected) {
                    failed = true;
                }
            }
        });
        for (uint32_t i = 0; i < 100000; ++i) {
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
        }
        consumer.join();
        require(!failed, "SPSC publication/order failure");
    }

    void test_mixer() {
        detail::mixer mixer;
        std::array<float, 512> output{};
        mixer.render(output.data(), 256, 1); // Finish initial gain ramp.
        auto *voice = new detail::sound{{0.2f, 0.3f, 0.4f, 0.5f}, 259};
        require(mixer.publish(voice), "Publish failed");
        mixer.render(output.data(), 8, 1);
        require(output[0] == 0 && output[5] == 0, "Scheduled clip played early");
        require(std::abs(output[6] - 0.2f) < 1e-6f && std::abs(output[9] - 0.5f) < 1e-6f, "Stereo/frame offset incorrect");
        require(output[10] == 0, "Clip overran its end");
        detail::sound *retired{};
        require(mixer.reclaim(retired) && retired == voice, "Voice was not returned to worker");
        delete retired;
        // Late publication skips elapsed samples instead of moving the sound's arrival.
        require(mixer.publish(new detail::sound{{0.1f, 0.1f, 0.2f, 0.2f, 0.3f, 0.3f, 0.4f, 0.4f}, 262}), "Late publish failed");
        mixer.render(output.data(), 4, 1);
        require(std::abs(output[0] - 0.3f) < 1e-6f, "Late clip did not skip elapsed audio");
        while (mixer.reclaim(retired)) {
            delete retired;
        }
        for (size_t i = 0; i < detail::voice_capacity; ++i) {
            require(mixer.publish(new detail::sound{std::vector<float>(512, 0.5f), 268}), "Voice capacity failed");
        }
        mixer.render(output.data(), 256, 1);
        for (auto sample : output) {
            require(std::isfinite(sample) && sample == 1, "Overlapping voices not limited");
        }
        size_t completed{};
        while (mixer.reclaim(retired)) {
            delete retired;
            ++completed;
        }
        require(completed == detail::voice_capacity, "Finished voices leaked");
        mixer.clear();
    }

    void test_callback_sizes() {
        const auto render = [](bool partitioned) {
            detail::mixer mixer;
            std::vector<float> samples(700 * channels);
            for (size_t i = 0; i < samples.size(); ++i) {
                samples[i] = float(int(i % 23) - 11) / 32;
            }
            require(mixer.publish(new detail::sound{samples, 17}), "Publish failed");
            require(mixer.publish(new detail::sound{samples, 293}), "Publish failed");
            std::vector<float> result(1200 * channels);
            const std::array<uint32_t, 6> sizes{1, 7, 63, 256, 3, 127};
            uint32_t cursor{};
            size_t block{};
            for (const auto boundary : {600u, 1200u}) {
                while (cursor < boundary) {
                    const auto count = std::min(boundary - cursor, partitioned ? sizes[block++ % sizes.size()] : boundary - cursor);
                    mixer.render(result.data() + size_t(cursor) * channels, count, boundary == 600 ? 1.0f : 0.0f);
                    cursor += count;
                }
            }
            detail::sound *retired{};
            size_t completed{};
            while (mixer.reclaim(retired)) {
                delete retired;
                ++completed;
            }
            require(completed == 2, "Callback size changed voice completion");
            mixer.clear();
            return result;
        };
        const auto whole = render(false);
        require(whole == render(true), "Callback size changed scheduling or gain smoothing");
        require(whole[599 * channels] != 0 && whole[600 * channels] != 0, "Mute was not smoothed");
        require(std::abs(whole[841 * channels]) < 1e-6f, "Mute ramp did not finish");
    }

    void test_engine(bool real_device) {
        engine audio({.max_clips = 1, .null_device = !real_device});
        const auto main_thread = std::this_thread::get_id();
        std::promise<std::thread::id> generated_on;
        std::promise<void> release_generator;
        auto release = release_generator.get_future().share();
        auto generated = generated_on.get_future();
        clip first{std::function<pcm()>{[&] {
            generated_on.set_value(std::this_thread::get_id());
            release.wait();
            return pcm{std::vector<float>(4800, 0), 0.05};
        }}};
        await([&] {
            return audio.submit(std::move(first));
        });
        require(generated.get() != main_thread, "Synthesis ran on calling thread");
        clip overflow{pcm{std::vector<float>(100, 0)}};
        const bool rejected = !audio.submit(std::move(overflow));
        const bool retained = std::get<pcm>(overflow.data).samples.size() == 100;
        release_generator.set_value();
        require(rejected && retained, "Queue overflow lost caller ownership");
        await([&] {
            return audio.stats().completed == 1 && audio.stats().outstanding == 0;
        });
        require(audio.stats().outstanding == 0, "Completed clip retained capacity");
        clip invalid{pcm{{std::numeric_limits<float>::quiet_NaN(), 0}}};
        await([&] {
            return audio.submit(std::move(invalid));
        });
        await([&] {
            return audio.stats().failed == 1 && audio.stats().outstanding == 0;
        });
        require(audio.stats().outstanding == 0, "Invalid clip retained capacity");
        clip throws{std::function<pcm()>{[]() -> pcm {
            throw std::runtime_error("Test generator failure");
        }}};
        await([&] {
            return audio.submit(std::move(throws));
        });
        await([&] {
            return audio.stats().failed == 2 && audio.stats().outstanding == 0;
        });
        audio.update({0});
        clip last{pcm{std::vector<float>(4800, 0), 120}};
        await([&] {
            return audio.submit(std::move(last));
        });
        // Destructor stops pending/delayed playback without waiting for its 120 second start.
    }

    void test_transport() {
        detail::mixer mixer;
        std::array<float, 512> output{};
        mixer.render(output.data(), 256, 1);
        pcm media{{0.1f, -0.1f, 0.2f, -0.2f, 0.3f, -0.3f, 0.4f, -0.4f}};
        mixer.load(media);
        mixer.render(output.data(), 2, 1);
        require(mixer.position() == 0 && output[0] == 0, "Loaded transport did not remain paused");
        mixer.pause(false);
        mixer.render(output.data(), 2, 1);
        require(output[0] == 0.1f && output[1] == -0.1f && output[2] == 0.2f, "Media channel/sample order changed");
        require(mixer.position() == 2.0 / sample_rate, "Media sample clock incorrect");
        mixer.pause(true);
        mixer.render(output.data(), 2, 1);
        require(output[0] == 0 && mixer.position() == 2.0 / sample_rate, "Pause advanced audio");
        mixer.seek(3);
        mixer.pause(false);
        mixer.render(output.data(), 2, 1);
        require(output[0] == 0.4f && output[2] == 0 && mixer.position() == 5.0 / sample_rate, "Seek/EOF silence incorrect");
        mixer.load(media);
        require(mixer.position() == 0, "Replacing media retained the old clock");
        mixer.clear();

        const auto converted = detail::convert_pcm(std::vector<float>(44100, 0.25f), 1, 44100);
        require(std::abs(int64_t(converted.samples.size() / channels) - sample_rate) <= 8, "Resampling duration incorrect");
        for (size_t i = 0; i < converted.samples.size(); i += channels) {
            require(converted.samples[i] == converted.samples[i + 1], "Mono/stereo conversion differs between channels");
        }
        engine audio({.null_device = true});
        audio.load(pcm{std::vector<float>(sample_rate * channels, 0.1f)});
        require(audio.position() == 0, "Engine started media before requested");
        audio.pause(false);
        await([&] {
            return audio.position() > 0.02;
        });
        audio.pause(true);
        // Allow an already running callback to finish before checking the paused cursor.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const auto paused = audio.position();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        require(audio.position() == paused, "Engine pause did not freeze its clock");
        audio.seek(0.5);
        await([&] {
            return audio.position() == 0.5;
        });
        audio.load(pcm{});
        require(audio.position() == 0, "Empty media did not clear the transport");
        audio.load(pcm{std::vector<float>(size_t(sample_rate) * channels * 301, 0.0f)});
        audio.seek(300.5);
        require(audio.position() == 300.5, "Long media seek was rejected or truncated");
        audio.seek(1055.0);
        require(audio.position() == 1055.0, "Seek beyond media EOF did not preserve the sample clock");
        bool rejected = false;
        try {
            audio.seek(std::numeric_limits<double>::max());
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        require(rejected, "Unrepresentable audio seek was accepted");
    }

    void test_media_edits() {
        using bytes = std::vector<uint8_t>;
        const auto u32 = [](bytes &data, uint32_t value) {
            for (int shift = 24; shift >= 0; shift -= 8) {
                data.push_back(static_cast<uint8_t>(value >> shift));
            }
        };
        const auto box = [&](std::string_view type, const bytes &payload) {
            bytes result;
            u32(result, static_cast<uint32_t>(payload.size() + 8));
            result.insert(result.end(), type.begin(), type.end());
            result.insert(result.end(), payload.begin(), payload.end());
            return result;
        };
        const auto append = [](bytes &target, const bytes &source) {
            target.insert(target.end(), source.begin(), source.end());
        };
        const auto track = [&](std::string_view handler, uint32_t rate, uint32_t media_time, uint32_t leading) {
            bytes mdhd(12, 0);
            u32(mdhd, rate);
            bytes hdlr(8, 0);
            hdlr.insert(hdlr.end(), handler.begin(), handler.end());
            auto mdia = box("mdhd", mdhd);
            append(mdia, box("hdlr", hdlr));
            bytes elst(4, 0);
            u32(elst, leading ? 2 : 1);
            if (leading) {
                u32(elst, leading);
                u32(elst, UINT32_MAX);
                u32(elst, 0x10000);
            }
            u32(elst, 4000);
            u32(elst, media_time);
            u32(elst, 0x10000);
            auto trak = box("mdia", mdia);
            append(trak, box("edts", box("elst", elst)));
            return box("trak", trak);
        };
        for (const auto leading : {0u, 500u}) {
            bytes mvhd(12, 0);
            u32(mvhd, 1000);
            auto movie = box("mvhd", mvhd);
            append(movie, track("vide", 12800, 1024, 0));
            append(movie, track("soun", 44100, 1024, leading));
            const auto file = box("moov", movie);
            std::istringstream stream{std::string(file.begin(), file.end())};
            detail::mp4_timeline timeline{stream, file.size()};
            require(std::abs(timeline.audio_origin(0.08) - (1024.0 / 44100 - leading / 1000.0)) < 1e-10, "Audio priming, video reorder delay or empty edit alignment incorrect");
        }
    }
} // namespace

int main(int argc, char **argv) {
    try {
        bool real_device{};
        std::filesystem::path media;
        double expected_onset{-1};
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg{argv[i]};
            if (arg == "--real-device") {
                real_device = true;
            } else if (arg == "--media" && i + 1 < argc) {
                media = argv[++i];
            } else if (arg == "--expect-onset" && i + 1 < argc) {
                expected_onset = std::stod(argv[++i]);
            } else {
                throw std::invalid_argument("Usage: vulkanizer_audio_test [--real-device] [--media path] [--expect-onset seconds]");
            }
        }
        if (!media.empty() && !std::filesystem::is_regular_file(media)) {
            std::cerr << "Missing media asset: " << media << '\n';
            return 77;
        }
        test_queue();
        test_mixer();
        test_callback_sizes();
        test_engine(real_device);
        test_transport();
        test_media_edits();
        if (!media.empty()) {
            const auto decoded = vkz::audio::load_media(media);
            require(decoded.has_value() && !decoded->samples.empty(), "Media audio track was not decoded");
            double energy{};
            for (const auto sample : decoded->samples) {
                require(std::isfinite(sample), "Decoded audio contains nonfinite PCM");
                energy += sample * sample;
            }
            std::cout << "Decoded media: " << decoded->samples.size() / vkz::audio::channels << " stereo frames at " << vkz::audio::sample_rate << " Hz, energy=" << energy << '\n';
            if (expected_onset >= 0) {
                const auto onset = std::find_if(decoded->samples.begin(), decoded->samples.end(), [](float sample) {
                    return std::abs(sample) >= 0.1f;
                });
                require(onset != decoded->samples.end(), "No audible pulse found");
                const auto seconds = double((onset - decoded->samples.begin()) / vkz::audio::channels) / vkz::audio::sample_rate;
                require(std::abs(seconds - expected_onset) < 0.02, "Audio onset differs from the video timeline");
                std::cout << "Pulse onset: " << seconds << " seconds\n";
            }
        }
        std::cout << "Audio tests passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
