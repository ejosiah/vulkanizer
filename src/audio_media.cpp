#include <vulkanizer/audio.hpp>
#include "audio_decode.hpp"
#include "audio_timeline.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

namespace vkz::audio {
    namespace {
        using Microsoft::WRL::ComPtr;

        void check(HRESULT result, const char *operation) {
            if (FAILED(result)) {
                throw std::runtime_error(std::string(operation) + " failed (HRESULT " + std::to_string(static_cast<uint32_t>(result)) + ")");
            }
        }

        class media_runtime {
          public:
            media_runtime() {
                const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                if (result != RPC_E_CHANGED_MODE) {
                    check(result, "COM initialization");
                    com_ = true;
                }
                const auto started = MFStartup(MF_VERSION, MFSTARTUP_FULL);
                if (FAILED(started)) {
                    if (com_) {
                        CoUninitialize();
                    }
                    check(started, "Media Foundation initialization");
                }
            }

            ~media_runtime() {
                MFShutdown();
                if (com_) {
                    CoUninitialize();
                }
            }

          private:
            bool com_{};
        };
    } // namespace

    std::optional<pcm> load_media(const std::filesystem::path &path) {
        if (!std::filesystem::is_regular_file(path)) {
            throw std::runtime_error("Audio media file does not exist: " + path.string());
        }
        media_runtime runtime;
        ComPtr<IMFSourceReader> reader;
        check(MFCreateSourceReaderFromURL(std::filesystem::absolute(path).c_str(), nullptr, &reader), "Opening audio media");
        ComPtr<IMFMediaType> native;
        const auto found = reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &native);
        if (found == MF_E_INVALIDSTREAMNUMBER) {
            return std::nullopt;
        }
        check(found, "Finding audio stream");

        // Read the first compressed video timestamp without changing the video decoder.
        LONGLONG origin{};
        ComPtr<IMFMediaType> video_type;
        if (SUCCEEDED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &video_type))) {
            DWORD flags{};
            ComPtr<IMFSample> sample;
            check(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &origin, &sample), "Reading video timeline origin");
        }
        const detail::mp4_timeline timeline{path};
        origin = static_cast<LONGLONG>(std::llround(timeline.audio_origin(double(origin) / 10000000.0) * 10000000.0));
        check(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE), "Disabling non-audio streams");
        check(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE), "Selecting audio stream");
        ComPtr<IMFMediaType> type;
        check(MFCreateMediaType(&type), "Creating audio format");
        check(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio), "Setting audio type");
        check(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float), "Setting PCM format");
        check(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type.Get()), "Decoding float PCM");
        check(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &type), "Reading PCM format");
        UINT32 rate{}, count{}, bits{};
        check(type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate), "Reading audio sample rate");
        check(type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &count), "Reading audio channels");
        check(type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits), "Reading audio sample format");
        if (!rate || !count || count > 32 || bits != 32) {
            throw std::runtime_error("Unsupported decoded audio format");
        }
        std::vector<float> samples;
        for (;;) {
            DWORD flags{};
            LONGLONG timestamp{};
            ComPtr<IMFSample> sample;
            check(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &timestamp, &sample), "Reading decoded audio");
            if (flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) {
                throw std::runtime_error("Audio decode failed or changed format");
            }
            if (sample) {
                ComPtr<IMFMediaBuffer> buffer;
                check(sample->ConvertToContiguousBuffer(&buffer), "Reading PCM buffer");
                DWORD length{};
                check(buffer->GetCurrentLength(&length), "Reading PCM buffer size");
                if (length % (sizeof(float) * count)) {
                    throw std::runtime_error("Incomplete PCM frame");
                }
                const auto frame = static_cast<int64_t>(std::llround(double(timestamp - origin) * rate / 10000000.0));
                const auto trim = static_cast<size_t>(std::min<int64_t>(std::max<int64_t>(0, -frame), length / (sizeof(float) * count)));
                const auto begin = static_cast<uint64_t>(std::max<int64_t>(frame, 0));
                const auto frames = length / (sizeof(float) * count) - trim;
                if (begin + frames > uint64_t(rate) * 300) {
                    throw std::runtime_error("Audio media exceeds 300 seconds");
                }
                samples.resize(std::max<uint64_t>(samples.size(), (begin + frames) * count), 0.0f);
                BYTE *bytes{};
                check(buffer->Lock(&bytes, nullptr, nullptr), "Mapping PCM buffer");
                if (frames) {
                    std::memcpy(samples.data() + begin * count, bytes + trim * count * sizeof(float), frames * count * sizeof(float));
                }
                check(buffer->Unlock(), "Unmapping PCM buffer");
            }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
                break;
            }
        }
        return detail::convert_pcm(samples, count, rate);
    }
} // namespace vkz::audio
#else
namespace vkz::audio {
    std::optional<pcm> load_media(const std::filesystem::path &) {
        throw std::runtime_error("Media-track audio decoding currently requires Windows Media Foundation");
    }
} // namespace vkz::audio
#endif
