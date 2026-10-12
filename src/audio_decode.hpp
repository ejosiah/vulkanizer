#pragma once
#include <vulkanizer/audio.hpp>
#include <span>

namespace vkz::audio::detail {
    pcm convert_pcm(std::span<const float> input, uint32_t channels, uint32_t sample_rate);
}
