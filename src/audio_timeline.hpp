#pragma once
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <string>
#include <cstdint>

namespace vkz::audio::detail {
    struct media_edit {
        uint32_t timescale{};
        int64_t media_time{};
        uint64_t leading_empty{};
        bool edited{};
    };

    // Source Reader's MP4 samples retain track timestamps. Apply the initial edit
    // independently for each track (AAC encoder priming differs from H.264 reordering).
    class mp4_timeline {
      public:
        explicit mp4_timeline(const std::filesystem::path &path) : owned_{path, std::ios::binary}, file_{owned_} {
            if (!file_) {
                throw std::runtime_error("Cannot read audio media timeline");
            }
            parse(std::filesystem::file_size(path));
        }

        mp4_timeline(std::istream &stream, uint64_t size) : file_{stream} {
            parse(size);
        }

        double audio_origin(double first_video) const {
            const auto audio_edit = offset(audio_);
            const auto video_edit = offset(video_);
            return audio_edit + first_video - video_edit;
        }

      private:
        void parse(uint64_t size) {
            boxes(0, size, [&](std::string_view type, uint64_t begin, uint64_t end) {
                if (type == "moov") {
                    boxes(begin, end, [&](std::string_view child, uint64_t payload, uint64_t limit) {
                        if (child == "mvhd") {
                            movie_rate_ = u32(payload + (byte(payload) == 1 ? 20 : 12));
                        } else if (child == "trak") {
                            track(payload, limit);
                        }
                    });
                }
            });
        }

        uint8_t byte(uint64_t offset) {
            file_.clear();
            file_.seekg(offset);
            const auto value = file_.get();
            return value == EOF ? 0 : static_cast<uint8_t>(value);
        }

        uint32_t u32(uint64_t offset) {
            uint32_t value{};
            for (uint32_t i = 0; i < 4; ++i) {
                value = (value << 8) | byte(offset + i);
            }
            return value;
        }

        uint64_t u64(uint64_t offset) {
            return (uint64_t(u32(offset)) << 32) | u32(offset + 4);
        }

        template <typename Function> void boxes(uint64_t begin, uint64_t end, Function visit) {
            for (auto position = begin; position <= end && end - position >= 8;) {
                uint64_t size = u32(position);
                uint64_t header = 8;
                if (size == 1) {
                    if (end - position < 16) {
                        return;
                    }
                    size = u64(position + 8);
                    header = 16;
                } else if (size == 0) {
                    size = end - position;
                }
                if (size < header || size > end - position) {
                    return;
                }
                char type[4];
                for (uint32_t i = 0; i < 4; ++i) {
                    type[i] = static_cast<char>(byte(position + 4 + i));
                }
                visit(std::string_view{type, 4}, position + header, position + size);
                position += size;
            }
        }

        void track(uint64_t begin, uint64_t end) {
            media_edit edit;
            std::string handler;
            boxes(begin, end, [&](std::string_view type, uint64_t payload, uint64_t limit) {
                if (type == "mdia") {
                    boxes(payload, limit, [&](std::string_view child, uint64_t data, uint64_t) {
                        if (child == "mdhd") {
                            edit.timescale = u32(data + (byte(data) == 1 ? 20 : 12));
                        } else if (child == "hdlr") {
                            for (uint32_t i = 0; i < 4; ++i) {
                                handler += static_cast<char>(byte(data + 8 + i));
                            }
                        }
                    });
                } else if (type == "edts") {
                    boxes(payload, limit, [&](std::string_view child, uint64_t data, uint64_t bound) {
                        if (child != "elst") {
                            return;
                        }
                        const auto version = byte(data);
                        const auto count = u32(data + 4);
                        const uint64_t stride = version == 1 ? 20 : 12;
                        if (version > 1 || bound - data < 8 || count > (bound - data - 8) / stride) {
                            throw std::runtime_error("Invalid MP4 edit list");
                        }
                        bool media_found{};
                        for (uint32_t i = 0; i < count; ++i) {
                            const auto item = data + 8 + i * stride;
                            const auto duration = version == 1 ? u64(item) : u32(item);
                            const auto time = version == 1 ? static_cast<int64_t>(u64(item + 8)) : static_cast<int32_t>(u32(item + 4));
                            const auto rate = u32(item + (version == 1 ? 16 : 8));
                            if (rate != 0x00010000 || time < -1 || media_found) {
                                throw std::runtime_error("Multiple/rate-adjusted MP4 media edits are unsupported");
                            }
                            if (time == -1) {
                                edit.leading_empty += duration;
                            } else {
                                edit.media_time = time;
                                media_found = true;
                            }
                        }
                        edit.edited = media_found;
                    });
                }
            });
            if (handler == "soun" && !audio_) {
                audio_ = edit;
            }
            if (handler == "vide" && !video_) {
                video_ = edit;
            }
        }

        double offset(const std::optional<media_edit> &value) const {
            if (!value || !value->edited || !value->timescale || !movie_rate_) {
                return 0;
            }
            return double(value->media_time) / value->timescale - double(value->leading_empty) / movie_rate_;
        }

        std::ifstream owned_;
        std::istream &file_;
        std::optional<media_edit> audio_;
        std::optional<media_edit> video_;
        uint32_t movie_rate_{};
    };
} // namespace vkz::audio::detail
