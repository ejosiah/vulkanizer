#pragma once

#include <vulkanizer/video.hpp>
#include "third_party/h264.hpp"
#include <array>

namespace vkz::video {
    struct clip::impl {
        struct access_unit {
            h264::SliceHeader slice{};
            std::vector<uint8_t> bytes;
            std::vector<uint32_t> slice_offsets;
        };

        uint32_t width{};
        uint32_t height{};
        uint32_t coded_width{};
        uint32_t coded_height{};
        uint32_t max_references{};
        double duration{};
        std::array<h264::SPS, 32> sps{};
        std::array<h264::PPS, 256> pps{};
        std::vector<uint32_t> sps_ids;
        std::vector<uint32_t> pps_ids;
        std::vector<access_unit> units;
        std::vector<frame_info> frames;
        std::vector<uint32_t> display_to_decode;
    };
} // namespace vkz::video
