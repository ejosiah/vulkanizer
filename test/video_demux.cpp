#include "video/third_party/minimp4.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    using bytes = std::vector<uint8_t>;

    void word(bytes &data, uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            data.push_back(static_cast<uint8_t>(value >> shift));
        }
    }

    bytes box(const char *type, const bytes &payload) {
        bytes data;
        word(data, static_cast<uint32_t>(payload.size() + 8));
        data.insert(data.end(), type, type + 4);
        data.insert(data.end(), payload.begin(), payload.end());
        return data;
    }

    int read(int64_t offset, void *output, size_t size, void *token) {
        const auto &data = *static_cast<const bytes *>(token);
        if (offset < 0 || static_cast<uint64_t>(offset) > data.size() || size > data.size() - static_cast<size_t>(offset)) {
            return 1;
        }
        std::memcpy(output, data.data() + offset, size);
        return 0;
    }

    void check(uint8_t version, std::array<int32_t, 4> offsets, bool accepted) {
        bytes ctts;
        word(ctts, uint32_t(version) << 24);
        word(ctts, static_cast<uint32_t>(offsets.size()));
        for (const auto offset : offsets) {
            word(ctts, 1);
            word(ctts, static_cast<uint32_t>(offset));
        }
        // Minimal container exercises the demuxer's FullBox version gate and CTTS
        // expansion without requiring local video assets or a GPU.
        auto file = box("moov", box("trak", box("ctts", ctts)));
        MP4D_demux_t demux{};
        const auto opened = MP4D_open(&demux, read, &file, static_cast<int64_t>(file.size()));
        bool valid = bool(opened) == accepted;
        if (opened && accepted) {
            valid = valid && demux.track_count == 1 && demux.track[0].composition_offset_count == offsets.size();
            if (valid) {
                for (size_t i = 0; i < offsets.size(); ++i) {
                    valid = valid && demux.track[0].composition_offset[i] == offsets[i];
                }
            }
        }
        MP4D_close(&demux);
        if (!valid) {
            throw std::runtime_error("CTTS version acceptance or signed offsets are incorrect");
        }
    }
} // namespace

int main() {
    try {
        check(0, {0, 2, 1, 1}, true);
        check(1, {0, 2, -1, -1}, true);
        check(2, {0, 2, -1, -1}, false);
        std::cout << "PASS CTTS unsigned version 0, signed version 1, and unsupported version rejection\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
