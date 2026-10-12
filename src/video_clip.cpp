#include <vulkanizer/video.hpp>
#include <vulkanizer/io.hpp>
#include "video/clip.hpp"

#define MINIMP4_IMPLEMENTATION
#include "video/third_party/minimp4.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace vkz::video {
    namespace {
        void require(bool value, const char *message) {
            if (!value) {
                throw std::runtime_error(message);
            }
        }

        int read_mp4(int64_t offset, void *buffer, size_t size, void *token) {
            const auto &bytes = *static_cast<const byte_string *>(token);
            if (offset < 0 || static_cast<uint64_t>(offset) > bytes.size() || size > bytes.size() - static_cast<size_t>(offset)) {
                return 1;
            }
            std::memcpy(buffer, bytes.data() + offset, size);
            return 0;
        }

        struct demux {
            ~demux() {
                MP4D_close(&value);
            }

            MP4D_demux_t value{};
        };

        // The GPU consumes EBSP. Only CPU header parsing removes emulation-prevention bytes.
        std::vector<uint8_t> rbsp(const uint8_t *data, size_t size) {
            std::vector<uint8_t> bytes;
            bytes.reserve(size);
            for (size_t i = 0; i < size; ++i) {
                if (i >= 2 && data[i] == 3 && data[i - 1] == 0 && data[i - 2] == 0) {
                    continue;
                }
                bytes.push_back(data[i]);
            }
            return bytes;
        }

        uint32_t big_endian(const uint8_t *data) {
            return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | data[3];
        }
    } // namespace

    clip clip::load(const path &path) {
        auto file = loadFile(path.string());
        require(!file.empty(), "Empty MP4 video");
        demux mp4;
        require(MP4D_open(&mp4.value, read_mp4, &file, file.size()) == 1, "Cannot demux MP4 video");
        uint32_t track_index = 0;
        while (track_index < mp4.value.track_count && mp4.value.track[track_index].handler_type != MP4D_HANDLER_TYPE_VIDE) {
            ++track_index;
        }
        require(track_index < mp4.value.track_count, "MP4 contains no video track");
        const auto &track = mp4.value.track[track_index];
        require(track.object_type_indication == MP4_OBJECT_TYPE_AVC, "Only MP4/H.264 video is supported");
        require(track.timescale && track.sample_count, "MP4 video has no samples or timescale");
        clip result;
        result.impl_ = std::make_shared<impl>();
        auto &out = *result.impl_;
        out.width = track.SampleDescription.video.width;
        out.height = track.SampleDescription.video.height;

        for (int i = 0, size = 0;; ++i) {
            const auto *data = static_cast<const uint8_t *>(MP4D_read_sps(&mp4.value, track_index, i, &size));
            if (!data) {
                break;
            }
            require(size >= 4, "Truncated H.264 SPS");
            auto bytes = rbsp(data, size);
            h264::Bitstream bits{};
            bits.init(bytes.data(), bytes.size());
            h264::NALHeader nal{};
            require(h264::read_nal_header(&nal, &bits) && nal.type == h264::NAL_UNIT_TYPE_SPS, "Invalid SPS NAL");
            h264::SPS sps{};
            sps.chroma_format_idc = 1;
            h264::read_sps(&sps, &bits);
            require(sps.seq_parameter_set_id >= 0 && sps.seq_parameter_set_id < 32, "Invalid SPS ID");
            require(sps.frame_mbs_only_flag && sps.pic_order_cnt_type == 0 && sps.chroma_format_idc == 1 && !sps.bit_depth_luma_minus8 && !sps.bit_depth_chroma_minus8,
                    "Video requires progressive 8-bit 4:2:0 H.264 with picture order count type 0");
            require(sps.num_ref_frames > 0 && sps.num_ref_frames <= 16, "Unsupported H.264 reference count");
            require(!sps.frame_crop_left_offset && !sps.frame_crop_top_offset, "Nonzero left/top H.264 cropping is unsupported");
            require(sps.log2_max_frame_num_minus4 >= 0 && sps.log2_max_frame_num_minus4 <= 12 && sps.log2_max_pic_order_cnt_lsb_minus4 >= 0 && sps.log2_max_pic_order_cnt_lsb_minus4 <= 12,
                    "Invalid H.264 frame/POC bit width");
            const uint32_t coded_width = (sps.pic_width_in_mbs_minus1 + 1) * 16;
            const uint32_t coded_height = (sps.pic_height_in_map_units_minus1 + 1) * 16;
            require(coded_width >= out.width && coded_height >= out.height && coded_width <= 16384 && coded_height <= 16384, "Invalid coded video dimensions");
            require(out.sps_ids.empty() || (out.coded_width == coded_width && out.coded_height == coded_height && out.sps[out.sps_ids.front()].profile_idc == sps.profile_idc),
                    "Mid-stream H.264 profile/dimension changes are unsupported");
            out.sps_ids.push_back(sps.seq_parameter_set_id);
            out.sps[sps.seq_parameter_set_id] = sps;
            out.coded_width = coded_width;
            out.coded_height = coded_height;
            out.max_references = std::max(out.max_references, uint32_t(sps.num_ref_frames));
        }
        require(!out.sps_ids.empty(), "MP4 contains no H.264 SPS");
        for (int i = 0, size = 0;; ++i) {
            const auto *data = static_cast<const uint8_t *>(MP4D_read_pps(&mp4.value, track_index, i, &size));
            if (!data) {
                break;
            }
            require(size >= 2, "Truncated H.264 PPS");
            auto bytes = rbsp(data, size);
            h264::Bitstream bits{};
            bits.init(bytes.data(), bytes.size());
            h264::NALHeader nal{};
            require(h264::read_nal_header(&nal, &bits) && nal.type == h264::NAL_UNIT_TYPE_PPS, "Invalid PPS NAL");
            h264::PPS pps{};
            h264::read_pps(&pps, &bits);
            require(pps.pic_parameter_set_id >= 0 && pps.pic_parameter_set_id < 256 && pps.seq_parameter_set_id >= 0 && pps.seq_parameter_set_id < 32, "Invalid PPS/SPS ID");
            require(std::find(out.sps_ids.begin(), out.sps_ids.end(), pps.seq_parameter_set_id) != out.sps_ids.end() && !pps.num_slice_groups_minus1,
                    "PPS references an absent SPS or unsupported slice groups");
            out.pps_ids.push_back(pps.pic_parameter_set_id);
            out.pps[pps.pic_parameter_set_id] = pps;
        }
        require(!out.pps_ids.empty(), "MP4 contains no H.264 PPS");
        out.units.resize(track.sample_count);
        out.frames.resize(track.sample_count);
        int previous_lsb = 0;
        int previous_msb = 0;
        for (uint32_t i = 0; i < track.sample_count; ++i) {
            unsigned size{}, timestamp{}, duration{};
            const auto offset = MP4D_frame_offset(&mp4.value, track_index, i, &size, &timestamp, &duration);
            require(offset <= file.size() && size <= file.size() - offset && duration, "Invalid MP4 sample range/duration");
            const auto *sample = reinterpret_cast<const uint8_t *>(file.data()) + offset;
            auto &unit = out.units[i];
            auto &info = out.frames[i];
            info.decode_index = i;
            // Preserve signed CTTS offsets rather than clamping negative PTS in the demux helper.
            const int64_t pts = int64_t(track.timestamp[i]) + (i < track.composition_offset_count ? track.composition_offset[i] : 0);
            info.timestamp = double(pts) / track.timescale;
            info.duration = double(duration) / track.timescale;
            for (size_t pos = 0; pos < size;) {
                require(size - pos >= 4, "Truncated MP4 NAL length");
                const auto length = big_endian(sample + pos);
                pos += 4;
                require(length && length <= size - pos, "Invalid MP4 NAL length");
                const auto *nal_bytes = sample + pos;
                pos += length;
                const auto type = nal_bytes[0] & 31;
                if (type != h264::NAL_UNIT_TYPE_CODED_SLICE_IDR && type != h264::NAL_UNIT_TYPE_CODED_SLICE_NON_IDR) {
                    continue;
                }
                auto bytes = rbsp(nal_bytes, length);
                h264::Bitstream bits{};
                bits.init(bytes.data(), bytes.size());
                h264::NALHeader nal{};
                require(h264::read_nal_header(&nal, &bits), "Invalid H.264 slice NAL");
                auto peek = bits;
                peek.ue();
                const auto slice_type = peek.ue();
                const auto pps_id = peek.ue();
                require(slice_type <= 9 && pps_id < 256 && std::find(out.pps_ids.begin(), out.pps_ids.end(), pps_id) != out.pps_ids.end(), "Slice references invalid PPS/type");
                h264::SliceHeader slice{};
                h264::read_slice_header(&slice, &nal, out.pps.data(), out.sps.data(), &bits);
                require(!slice.field_pic_flag && !slice.drpm.long_term_reference_flag, "Interlaced pictures and long-term references are unsupported");
                require(!slice.delta_pic_order_cnt_bottom, "Different top/bottom picture order counts are unsupported");
                if (slice.drpm.adaptive_ref_pic_marking_mode_flag) {
                    for (const auto operation : slice.drpm.memory_management_control_operation) {
                        if (operation == 0) {
                            break;
                        }
                        if (operation != 1) {
                            throw std::runtime_error("Unsupported MMCO " + std::to_string(operation) + " at decode frame " + std::to_string(i) + " in " + path.string());
                        }
                    }
                }
                if (unit.slice_offsets.empty()) {
                    unit.slice = slice;
                    info.reference = nal.idc != 0;
                    info.idr = type == h264::NAL_UNIT_TYPE_CODED_SLICE_IDR;
                } else {
                    require(slice.frame_num == unit.slice.frame_num && slice.pic_order_cnt_lsb == unit.slice.pic_order_cnt_lsb && slice.pic_parameter_set_id == unit.slice.pic_parameter_set_id,
                            "MP4 sample contains more than one H.264 picture");
                }
                unit.slice_offsets.push_back(static_cast<uint32_t>(unit.bytes.size()));
                unit.bytes.insert(unit.bytes.end(), {0, 0, 0, 1});
                unit.bytes.insert(unit.bytes.end(), nal_bytes, nal_bytes + length);
            }
            require(!unit.slice_offsets.empty(), "MP4 sample contains no H.264 picture");
            require(i || info.idr, "Video must start with an IDR picture");
            const auto &sps = out.sps[out.pps[unit.slice.pic_parameter_set_id].seq_parameter_set_id];
            if (info.idr) {
                previous_lsb = 0;
                previous_msb = 0;
            }
            const auto max_lsb = 1 << (sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
            const auto lsb = unit.slice.pic_order_cnt_lsb;
            auto msb = previous_msb;
            if (!info.idr && lsb < previous_lsb && previous_lsb - lsb >= max_lsb / 2) {
                msb += max_lsb;
            } else if (!info.idr && lsb > previous_lsb && lsb - previous_lsb > max_lsb / 2) {
                msb -= max_lsb;
            }
            info.picture_order_count = msb + lsb;
            if (info.reference) {
                previous_lsb = lsb;
                previous_msb = msb;
            }
        }
        out.display_to_decode.resize(out.frames.size());
        std::iota(out.display_to_decode.begin(), out.display_to_decode.end(), 0u);
        std::stable_sort(out.display_to_decode.begin(), out.display_to_decode.end(), [&](auto a, auto b) {
            return out.frames[a].timestamp < out.frames[b].timestamp;
        });
        const auto first_pts = out.frames[out.display_to_decode.front()].timestamp;
        for (uint32_t order = 0; order < out.frames.size(); ++order) {
            auto &frame = out.frames[out.display_to_decode[order]];
            frame.display_order = order;
            frame.timestamp -= first_pts;
            out.duration = std::max(out.duration, frame.timestamp + frame.duration);
        }
        return result;
    }

    uint32_t clip::width() const {
        return impl_->width;
    }

    uint32_t clip::height() const {
        return impl_->height;
    }

    double clip::duration() const {
        return impl_->duration;
    }

    std::span<const frame_info> clip::frames() const {
        return impl_->frames;
    }

    uint32_t clip::frame_at(double seconds) const {
        require(std::isfinite(seconds), "Playback time must be finite");
        const auto &order = impl_->display_to_decode;
        const auto it = std::upper_bound(order.begin(), order.end(), seconds, [&](double time, uint32_t index) {
            return time < impl_->frames[index].timestamp;
        });
        return it == order.begin() ? 0u : static_cast<uint32_t>(it - order.begin() - 1);
    }
} // namespace vkz::video
