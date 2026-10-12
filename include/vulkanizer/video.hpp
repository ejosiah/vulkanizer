#pragma once

#include "texture.hpp"

#include <memory>
#include <span>
#include <vector>

namespace vkz::video {
    struct frame_info {
        uint32_t decode_index{};
        uint32_t display_order{};
        double timestamp{};
        double duration{};
        int32_t picture_order_count{};
        bool reference{};
        bool idr{};
    };

    // CPU-only MP4/H.264 demux. Parameter/slice parsers are private implementation details.
    class clip {
      public:
        static clip load(const path &path);
        [[nodiscard]] uint32_t width() const;
        [[nodiscard]] uint32_t height() const;
        [[nodiscard]] double duration() const;
        [[nodiscard]] std::span<const frame_info> frames() const;
        [[nodiscard]] uint32_t frame_at(double seconds) const;

      private:
        friend class decoder;
        struct impl;
        std::shared_ptr<impl> impl_;
    };

    // Request these extensions and a vkz::queue::video_decode queue before device creation.
    [[nodiscard]] std::span<const char *const> required_extensions();
    [[nodiscard]] uint32_t decode_queue_family(VkPhysicalDevice physical_device);

    struct decoder_params {
        vma_memory_allocator *allocator{};
        VkQueue graphics_queue{};
        uint32_t graphics_queue_family{};
        VkQueue decode_queue{};
        uint32_t decode_queue_family{};
    };

    class decoder {
      public:
        decoder(const decoder_params &params, const clip &clip);
        ~decoder();
        decoder(const decoder &) = delete;
        decoder &operator=(const decoder &) = delete;

        // Decode/resolve in decode order, retaining RGB outputs until their presentation time.
        // Calls are synchronous. Invoke outside rendering, on the queue-owning thread.
        // Returned textures are borrowed; do not destroy them. An output survives until
        // a later frame()/reset() call. Finish draws using the old output before those calls.
        [[nodiscard]] const texture &frame(uint32_t display_order);
        void reset();
        [[nodiscard]] std::span<const uint32_t> decoded_frames() const;
        [[nodiscard]] const clip &source() const;

      private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };
} // namespace vkz::video
