#pragma once

#include <vulkanizer/builders.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/compute_pipeline_builder.hpp>
#include <cmrc/cmrc.hpp>
#include <cstring>

CMRC_DECLARE(rtx_shaders);

namespace rtx {
    inline std::vector<uint32_t> load_shader(const char *name) {
        const auto file = cmrc::rtx_shaders::get_filesystem().open(name);
        std::vector<uint32_t> words(file.size() / sizeof(uint32_t));
        std::memcpy(words.data(), file.begin(), file.size());
        return words;
    }

    // The application profiles the complete NRC and NRD passes.
    template <class Body>
    void profile_pass(VkCommandBuffer, const char *, Body &&body) {
        body();
    }

    inline void synchronize(VkCommandBuffer cmd) {
        vkz::barrier::push_and_flush(cmd, vkz::pipeline_stage_all_commands, vkz::pipeline_stage_all_commands, vkz::access_memory_read_write, vkz::access_memory_read_write);
    }
}
