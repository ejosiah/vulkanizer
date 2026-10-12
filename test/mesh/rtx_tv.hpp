#pragma once

#include <vulkanizer/video.hpp>
#include <vulkanizer/audio.hpp>
#include <vulkanizer/mesh.hpp>
#include <vulkanizer/descriptors.hpp>
#include <vulkanizer/pipeline.hpp>
#include <chrono>
#include <functional>

namespace rtx {
    // Persistent RGB output keeps all raster and ray tracing descriptors stable.
    class tv_light {
    public:
        static constexpr float radiance_scale = 20;

        tv_light(vkz::vma_memory_allocator &allocator, VkQueue queue, uint32_t queue_family,
            const vkz::path &video_path, std::span<const uint32_t> filter_shader);
        ~tv_light();

        void configure(vkz::mesh::data &scene);
        void new_frame();
        void frame_submitted();
        void update_strength(float value);
        void verify();
        void verify_lighting(vkz::texture &color, const std::function<void(VkCommandBuffer)> &render);

        const vkz::texture &screen() const { return screen_; }
        const vkz::texture &filtered() const { return filtered_; }
        const vkz::texture &ltc_matrix() const { return ltc_matrix_; }
        const vkz::texture &ltc_amplitude() const { return ltc_amplitude_; }
        const vkz::buffer &parameters() const { return parameters_; }

    private:
        vkz::texture create_texture(uint32_t width, uint32_t height, uint32_t layers = 1);
        vkz::texture load_table(std::span<const float> values);
        void filter_frame(const vkz::texture &frame);

        vkz::vma_memory_allocator &allocator_;
        vkz::device device_;
        VkQueue queue_;
        uint32_t queue_family_;
        std::unique_ptr<vkz::video::decoder> decoder_;
        std::unique_ptr<vkz::audio::engine> audio_;
        vkz::texture screen_, filtered_, ltc_matrix_, ltc_amplitude_;
        vkz::buffer parameters_;
        vkz::descriptor_set_layout filter_layout_;
        vkz::descriptor_pool filter_pool_;
        vkz::descriptor_set filter_set_;
        vkz::pipeline filter_pipeline_;

        struct area_parameters {
            std::array<glm::vec4, 4> points{};
            glm::vec4 settings{0, radiance_scale, 0, 0}; // enabled, radiance scale
        } data_;

        std::chrono::steady_clock::time_point tick_;
        double playback_time_{};
        double sync_window_{};
        double max_av_drift_{};
        uint32_t playback_loops_{};
        bool clock_started_{};
        bool resume_audio_{};
        uint32_t last_frame_{~0u};
        uint32_t decoded_frames_{};
    };
}
