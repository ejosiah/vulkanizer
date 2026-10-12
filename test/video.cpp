#define VKZ_IOSTREAM_ADAPTER
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <vulkanizer/application.hpp>
#include <vulkanizer/video.hpp>
#include <vulkanizer/audio.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string_view>
#include <string>
#include <vector>

namespace {
    constexpr std::array<const char *, 3> filenames{"2616637-hd_1920_1080_30fps.mp4", "3116737-hd_1920_1080_25fps.mp4", "855289-hd_1920_1080_25fps.mp4"};
    constexpr std::array<const char *, 3> labels{"2616637 (29.97 fps)", "Cat (25 fps)", "Squirrel (25 fps)"};
    using clock = std::chrono::steady_clock;

    struct entry {
        std::filesystem::path path;
        std::string label;
    };

    struct options {
        std::filesystem::path directory{VKZ_VIDEO_RESOURCE_DIR};
        std::filesystem::path capture_directory;
        std::filesystem::path trace_directory;
        bool verify{};
        bool verify_playback{};
        bool parser_only{};
        double timeout{};
        int video{};
        std::vector<std::filesystem::path> video_paths;
        std::vector<entry> videos;
        bool silent{};
    };

    vkz::vulkan_app_create_info settings() {
        vkz::vulkan_app_create_info info{};
        info.width = 1280;
        info.height = 800;
        info.title = "Vulkanizer video playback";
        info.depth_testing = false;
        info.validation = true;
        info.vsync = false;
        info.queues.push_back(vkz::queue::video_decode);
        const auto extensions = vkz::video::required_extensions();
        info.device_extensions.assign(extensions.begin(), extensions.end());
        info.extension_chain.add(VkPhysicalDeviceSamplerYcbcrConversionFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES, nullptr, VK_TRUE});
        return info;
    }

    void check(bool valid, const char *error) {
        if (!valid) {
            throw std::runtime_error(error);
        }
    }

    void check_clip(const vkz::video::clip &clip) {
        check(clip.width() > 0 && clip.height() > 0, "Invalid video dimensions");
        std::set<uint32_t> display_orders;
        double last_pts = -1;
        for (const auto &frame : clip.frames()) {
            check(frame.duration > 0 && frame.timestamp >= 0, "Invalid presentation timing");
            check(display_orders.insert(frame.display_order).second, "Duplicate presentation order");
            check(clip.frame_at(frame.timestamp + frame.duration * 0.1) == frame.display_order, "frame_at does not respect MP4 presentation order");
        }
        check(clip.frame_at(-1) == 0 && clip.frame_at(clip.duration() + 1) == clip.frames().size() - 1, "Playback endpoints are incorrect");
        for (uint32_t order = 0; order < clip.frames().size(); ++order) {
            const auto it = std::find_if(clip.frames().begin(), clip.frames().end(), [order](const auto &frame) {
                return frame.display_order == order;
            });
            check(it != clip.frames().end() && it->timestamp > last_pts, "Presentation timestamps are not strictly increasing");
            last_pts = it->timestamp;
        }
    }

    class video_test final : public vkz::application {
      public:
        explicit video_test(options value) : application(settings()), options_(std::move(value)), selected_(options_.video) {
        }

        ~video_test() {
            vkz::batch_submission{app_.graphics_queue()}.wait_idle();
            remove_descriptors();
            decoder_.reset();
        }

        void setup() override {
            const auto family = vkz::video::decode_queue_family(device_.physical);
            decoder_params_ = {&allocator_, app_.graphics_queue(), app_.queue_family_index(), vkz::get_queue(device_, family), family};
            load_clip(selected_);
            start_ = clock::now();
        }

        void new_frame() override {
            // application has waited this frame slot's fence; no draws still use the old texture.
            if (pending_selection_ >= 0) {
                load_clip(pending_selection_);
                pending_selection_ = -1;
            }
            const auto now = clock::now();
            const auto dt = clock_started_ ? std::chrono::duration<double>(now - tick_).count() : 0.0;
            tick_ = now;
            if (!options_.verify && !seek_pending_) {
                if (playing_) {
                    auto next = time_ + dt;
                    if (has_audio_ && clock_started_) {
                        // Audio positions arrive in callback blocks. Interpolate between
                        // them, gently correct phase and stay within half a video frame.
                        const auto sample_clock = audio_->position();
                        next += (sample_clock - next) * -std::expm1(-dt / 0.1);
                        next = std::clamp(next, sample_clock - sync_window_, sample_clock + sync_window_);
                    }
                    time_ = std::min(std::max(time_, next), decoder_->source().duration());
                }
                if (time_ >= decoder_->source().duration()) {
                    playing_ = false;
                    if (has_audio_) {
                        audio_->pause(true);
                    }
                }
            }
            if (!options_.verify) {
                order_ = decoder_->source().frame_at(time_);
            }
            output_ = decoder_->frame(order_);
            auto [it, inserted] = descriptors_.try_emplace(output_.image_view.handle);
            if (inserted) {
                it->second = vkz::imgui::add_texture(output_.sampler, output_.image_view, output_.image.layout);
            }
            current_descriptor_ = it->second;
        }

        void app_logic(VkCommandBuffer) override {
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Video", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            if (ImGui::BeginCombo("Video", options_.videos[selected_].label.c_str())) {
                for (int i = 0; i < static_cast<int>(options_.videos.size()); ++i) {
                    if (ImGui::Selectable(options_.videos[i].label.c_str(), selected_ == i)) {
                        pending_selection_ = i;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button(playing_ ? "Pause" : "Play")) {
                if (!playing_ && time_ >= decoder_->source().duration()) {
                    pending_selection_ = selected_;
                }
                playing_ = !playing_;
                if (has_audio_) {
                    audio_->pause(!playing_);
                    if (!playing_) {
                        audio_->seek(time_);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Restart")) {
                pending_selection_ = selected_;
            }
            ImGui::SameLine();
            ImGui::Text("%.2f / %.2f s | Frame %u / %zu", time_, decoder_->source().duration(), order_ + 1, decoder_->source().frames().size());
            auto seek = static_cast<float>(time_);
            if (ImGui::SliderFloat("Position", &seek, 0, static_cast<float>(decoder_->source().duration()))) {
                time_ = seek;
                if (has_audio_) {
                    audio_->pause(true);
                    audio_->seek(time_);
                }
                seek_pending_ = true;
            }
            ImGui::TextUnformatted(has_audio_ ? "Audio track loaded" : "No audio track / silent playback");
            if (has_audio_ && ImGui::SliderFloat("Volume", &volume_, 0, 1)) {
                audio_->update({volume_});
            }
            const auto available = ImGui::GetContentRegionAvail();
            const auto scale = std::min(available.x / decoder_->source().width(), available.y / decoder_->source().height());
            const ImVec2 size{decoder_->source().width() * scale, decoder_->source().height() * scale};
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - size.x) * 0.5f);
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(current_descriptor_)), size);
            ImGui::End();
            if (!options_.capture_directory.empty() && (order_ == 0 || order_ == decoder_->source().frames().size() / 2 || order_ == decoder_->source().frames().size() - 1) &&
                captured_.insert(order_).second) {
                capture();
            }
        }

        void frame_submitted() override {
            const auto now = clock::now();
            if (!clock_started_) {
                tick_ = now;
                clock_started_ = true;
                if (has_audio_) {
                    audio_->pause(!playing_);
                }
            }
            const auto wall = std::chrono::duration<double>(now - clip_start_).count();
            if (seek_pending_) {
                seek_pending_ = false;
                if (has_audio_) {
                    audio_->pause(!playing_);
                }
            }
            if (has_audio_) {
                max_av_drift_ = std::max(max_av_drift_, std::abs(audio_->position() - time_));
            }
            if (trace_) {
                trace_ << renders_ << ',' << wall << ',' << time_ << ',' << order_ << ',' << (has_audio_ ? audio_->position() : -1) << '\n';
            }
            if (last_order_ != order_) {
                if (last_order_ != UINT32_MAX) {
                    missing_ += order_ > last_order_ ? order_ - last_order_ - 1 : 1;
                    max_hold_ = std::max(max_hold_, wall - last_transition_);
                }
                last_order_ = order_;
                last_transition_ = wall;
                ++displayed_;
            }
            ++renders_;
        }

        void end_frame() override {
            if (options_.timeout > 0 && std::chrono::duration<double>(clock::now() - start_).count() >= options_.timeout) {
                glfwSetWindowShouldClose(app_.window(), GLFW_TRUE);
                return;
            }
            const auto count = decoder_->source().frames().size();
            const auto finished = options_.verify ? order_ == count - 1 : time_ >= decoder_->source().duration();
            if (!(options_.verify || options_.verify_playback)) {
                return;
            }
            if (finished) {
                check(displayed_ == count && missing_ == 0, "Playback skipped or repeated frame transitions");
                if (options_.verify_playback) {
                    const auto frames = decoder_->source().frames();
                    const auto shortest = std::min_element(frames.begin(), frames.end(), [](const auto &a, const auto &b) {
                        return a.duration < b.duration;
                    });
                    check(max_hold_ <= shortest->duration * 1.5, "Playback held a frame beyond the stutter threshold");
                    check(!has_audio_ || max_av_drift_ <= shortest->duration, "Audio/video clocks drifted by more than one video frame");
                }
                const auto decoded = decoder_->decoded_frames();
                check(decoded.size() == count, "Decoder did not process every source frame");
                for (size_t i = 0; i < decoded.size(); ++i) {
                    check(decoded[i] == i, "Duplicate or missing decode index");
                }
                std::cout << "PASS " << options_.videos[selected_].path.filename().string() << ": " << displayed_ << '/' << count << " frames, missing=" << missing_
                          << ", max_hold_ms=" << max_hold_ * 1000 << '\n';
                // Exercise restart and backward seeking against the same session.
                if (options_.verify) {
                    check(decoder_->frame(0).image.handle != VK_NULL_HANDLE, "Restart failed");
                    check(decoder_->frame(2).image.handle != VK_NULL_HANDLE, "Forward seek failed");
                    check(decoder_->frame(1).image.handle != VK_NULL_HANDLE, "Backward seek failed");
                }
                if (selected_ + 1 < static_cast<int>(options_.videos.size())) {
                    pending_selection_ = selected_ + 1;
                } else {
                    glfwSetWindowShouldClose(app_.window(), GLFW_TRUE);
                }
            } else if (options_.verify) {
                ++order_;
            }
        }

      private:
        void remove_descriptors() {
            for (const auto &[view, set] : descriptors_) {
                vkz::imgui::remove_texture(set);
            }
            descriptors_.clear();
        }

        void load_clip(int selection) {
            if (audio_) {
                audio_->pause(true);
            }
            has_audio_ = false;
            remove_descriptors();
            decoder_.reset();
            selected_ = selection;
            const auto &path = options_.videos[selected_].path;
            const auto clip = vkz::video::clip::load(path);
            check_clip(clip);
            const auto frames = clip.frames();
            const auto shortest = std::min_element(frames.begin(), frames.end(), [](const auto &a, const auto &b) {
                return a.duration < b.duration;
            });
            sync_window_ = shortest->duration * 0.5;
            decoder_ = std::make_unique<vkz::video::decoder>(decoder_params_, clip);
            output_ = decoder_->frame(0);
            if (!options_.verify && !options_.silent) {
                auto pcm = vkz::audio::load_media(path);
                if (pcm) {
                    if (!audio_) {
                        audio_ = std::make_unique<vkz::audio::engine>(vkz::audio::params{.null_device = options_.verify_playback});
                    }
                    audio_->load(std::move(*pcm));
                    audio_->update({volume_});
                    has_audio_ = true;
                }
            }
            order_ = 0;
            time_ = 0;
            playing_ = true;
            clock_started_ = false;
            displayed_ = 0;
            renders_ = 0;
            missing_ = 0;
            last_order_ = UINT32_MAX;
            max_hold_ = 0;
            max_av_drift_ = 0;
            seek_pending_ = false;
            captured_.clear();
            if (trace_.is_open()) {
                trace_.close();
            }
            if (!options_.trace_directory.empty()) {
                std::filesystem::create_directories(options_.trace_directory);
                trace_.open(options_.trace_directory / (path.filename().string() + ".csv"));
                check(bool(trace_), "Cannot open frame trace");
                trace_.precision(9);
                trace_ << "render_frame,wall_seconds,playback_seconds,display_order,audio_seconds\n";
            }
            clip_start_ = clock::now();
            std::cout << "Loaded " << path.filename().string() << ": " << clip.frames().size() << " frames, " << clip.duration() << " seconds, audio=" << (has_audio_ ? "yes" : "no") << '\n';
        }

        void capture() {
            const auto width = decoder_->source().width();
            const auto height = decoder_->source().height();
            auto readback = vkz::buffer::builder(allocator_)
                .size(width * height * 4)
                .usage(vkz::buffer::usage::transfer_dst)
                .memory_usage(VMA_MEMORY_USAGE_GPU_TO_CPU)
                .build();
            vkz::command_pool commands(device_, app_.queue_family_index(), vkz::command_pool_flags::transient, app_.graphics_queue());
            auto command = commands.create_command_buffer();
            vkz::copy(command, output_.image, readback);
            vkz::barrier::transfer_write_to_host_read(command);
            vkz::barrier::push_and_flush(command, output_.image, {static_cast<VkImageAspectFlags>(vkz::image::aspect::color), 0, 1, 0, 1}, vkz::pipeline::stage::transfer,
                                         vkz::pipeline::stage::fragment_shader, vkz::access::transfer_read, vkz::access::shader_sampled_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            commands.submit_and_wait(command);
            auto mapping = readback.map();
            readback.invalidate();
            std::filesystem::create_directories(options_.capture_directory);
            const auto path = options_.capture_directory / (std::to_string(selected_) + "_" + std::to_string(order_) + ".png");
            const auto saved = stbi_write_png(path.string().c_str(), width, height, 4, mapping._, width * 4);
            mapping.unmap();
            readback.destroy();
            check(saved != 0, "Cannot save decoded frame capture");
        }

        options options_;
        vkz::video::decoder_params decoder_params_{};
        std::unique_ptr<vkz::video::decoder> decoder_;
        vkz::texture output_{};
        std::map<VkImageView, VkDescriptorSet> descriptors_;
        std::set<uint32_t> captured_;
        VkDescriptorSet current_descriptor_{};
        clock::time_point start_{};
        clock::time_point clip_start_{};
        clock::time_point tick_{};
        std::ofstream trace_;
        uint32_t order_{};
        uint32_t last_order_{UINT32_MAX};
        uint32_t displayed_{};
        uint32_t missing_{};
        uint64_t renders_{};
        double time_{};
        double max_hold_{};
        double last_transition_{};
        int selected_{};
        int pending_selection_{-1};
        bool playing_{true};
        bool clock_started_{};
        std::unique_ptr<vkz::audio::engine> audio_;
        bool has_audio_{};
        float volume_{0.8f};
        bool seek_pending_{};
        double max_av_drift_{};
        double sync_window_{};
    };
} // namespace

int main(int argc, char **argv) {
    vkz::iostream_adapter::install(std::cout);
    try {
        options options;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg{argv[i]};
            if (arg == "--verify") {
                options.verify = true;
            } else if (arg == "--verify-playback") {
                options.verify_playback = true;
            } else if (arg == "--parser-only") {
                options.parser_only = true;
            } else if (arg == "--silent") {
                options.silent = true;
            } else if (i + 1 < argc && arg == "--video-path") {
                options.video_paths.emplace_back(argv[++i]);
            } else if (i + 1 < argc && arg == "--video-dir") {
                options.directory = argv[++i];
            } else if (i + 1 < argc && arg == "--capture-dir") {
                options.capture_directory = argv[++i];
            } else if (i + 1 < argc && arg == "--trace-dir") {
                options.trace_directory = argv[++i];
            } else if (i + 1 < argc && arg == "--timeout") {
                options.timeout = std::stod(argv[++i]);
            } else if (i + 1 < argc && arg == "--video") {
                options.video = std::stoi(argv[++i]);
            } else {
                throw std::invalid_argument("Usage: vulkanizer_video_test [--video-dir path] [--video-path path] [--video index] [--silent] [--verify|--verify-playback|--parser-only] [--timeout "
                                            "seconds] [--trace-dir path] [--capture-dir path]");
            }
        }
        for (size_t i = 0; i < filenames.size(); ++i) {
            const auto path = options.directory / filenames[i];
            if (std::filesystem::is_regular_file(path)) {
                options.videos.push_back({std::filesystem::absolute(path), labels[i]});
            } else if (options.video_paths.empty()) {
                std::cerr << "Missing video asset: " << path << '\n';
                return 77;
            }
        }
        for (const auto &path : options.video_paths) {
            check(std::filesystem::is_regular_file(path), "Supplied video path does not exist");
            const auto absolute = std::filesystem::weakly_canonical(path);
            const auto found = std::find_if(options.videos.begin(), options.videos.end(), [&](const entry &video) {
                return std::filesystem::equivalent(video.path, absolute);
            });
            if (found == options.videos.end()) {
                options.video = static_cast<int>(options.videos.size());
                options.videos.push_back({absolute, absolute.filename().string()});
            } else {
                options.video = static_cast<int>(found - options.videos.begin());
            }
        }
        check(options.video >= 0 && options.video < static_cast<int>(options.videos.size()), "--video index is out of range");
        if (options.parser_only) {
            for (const auto &video : options.videos) {
                const auto clip = vkz::video::clip::load(video.path);
                check_clip(clip);
                std::cout << "PASS parser " << video.path.filename().string() << ": " << clip.frames().size() << " frames\n";
            }
            return 0;
        }
        video_test app{options};
        app.main_loop();
    } catch (const std::exception &error) {
        std::cerr << "Video test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
