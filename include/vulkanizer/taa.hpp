#pragma once

#include "context.hpp"
#include "texture.hpp"
#include <glm/glm.hpp>
#include <cstdint>

namespace vkz::taa {
    using id = uint32_t;
    inline constexpr id invalid_id = 0;

    enum class history_sampling_filter : uint32_t {
        single,
        catmull_rom
    };
    enum class sub_sample_filter : uint32_t {
        none,
        mitchell,
        blackman_harris,
        catmull_rom
    };
    enum class history_constraint : uint32_t {
        none,
        clamp,
        clip,
        variance_clip,
        variance_clip_clamp
    };

    struct settings {
        bool operator==(const settings &) const = default;

        history_sampling_filter history_filter{history_sampling_filter::catmull_rom};
        sub_sample_filter sample_filter{sub_sample_filter::mitchell};
        history_constraint constraint{history_constraint::clamp};
        bool temporal_filtering{true};
        bool inverse_luminance_filtering{true};
        bool luminance_difference_filtering{true};
        bool full_taa{true};
        float history_weight{0.9f};
        float depth_threshold{0.001f};
    };

    // No dependency on a camera controller. Vulkan depth is [0,1], with 1 at the far plane.
    struct camera {
        glm::mat4 view_projection{1}; // Unjittered matrix.
        glm::vec2 jitter{};           // Pixel offset applied to the raster projection for this frame.
    };

    struct params {
        vkz::device device;
        vma_memory_allocator memory_allocator;
        // Borrowed, single-sample 2D inputs, matching extents. Color must be RGBA16F or RGBA32F,
        // with SAMPLED and TRANSFER_DST usage. Depth needs SAMPLED usage (R or RGBA are also supported).
        texture color;
        texture depth;
        uint32_t depth_channel{0};
        uint32_t in_flight_frames{2};
        settings options{};
    };

    id create(const params &params);
    // Caller must finish outstanding GPU work before destroy/resize. Inputs remain caller-owned.
    void destroy(id id);
    void resize(id id, const texture &color, const texture &depth);
    void configure(id id, const settings &options);
    void reset(id id);
    void update(id id, const camera &camera, uint32_t current_frame);
    // Call outside a render pass, on one graphics/compute+transfer queue, once per frame.
    // Inputs must already be readable in their supplied layouts. Resolves back into color and
    // restores that layout; history/velocity are internally synchronized across frames.
    void resolve(id id, VkCommandBuffer command_buffer);
    texture motion_vectors(id id);

    // Halton(2,3), 16-frame cycle, in pixels. Apply before drawing the scene, not to UI.
    glm::vec2 jitter(uint64_t frame_index);
    glm::mat4 jitter_projection(const glm::mat4 &projection, glm::vec2 pixel_offset, glm::uvec2 resolution);
} // namespace vkz::taa
