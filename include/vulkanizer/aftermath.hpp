#pragma once

#include <filesystem>

namespace vkz::aftermath {
    [[nodiscard]] bool available();

    // Must be called before creating a Vulkan device. Calling it more than once is safe.
    bool enable(const std::filesystem::path& output_directory = "aftermath");

    void disable();
}
