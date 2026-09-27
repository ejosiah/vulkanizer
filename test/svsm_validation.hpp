#pragma once
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <vulkanizer/log.hpp>

namespace svsm_test {
    inline std::atomic_uint validation_errors{};
    inline void install_logger() {
        vkz::iostream_adapter::install(std::cout);
        vkz::set_logger([](vkz::LogLevel level, std::string_view message) {
            vkz::iostream_adapter::log(level, message);
            if (level == vkz::LogLevel::Error &&
                (message.find("Validation Error") != std::string_view::npos || message.find("VUID-") != std::string_view::npos ||
                 message.find("SYNC-HAZARD") != std::string_view::npos))
                ++validation_errors;
        });
    }
    inline void check_validation() {
        if (validation_errors.load())
            throw std::runtime_error("SVSM run reported Vulkan validation errors");
    }
} // namespace svsm_test
