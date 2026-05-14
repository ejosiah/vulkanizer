#include "vulkanizer/log.hpp"

#include <stdexcept>

namespace vkz {
    namespace {
        LogFn g_logger = nullptr;
    }

    void set_logger(LogFn fn) {
        g_logger = fn;
    }

    void log(LogLevel level, std::string_view message) {
        if (g_logger) {
            g_logger(level, message);
            return;
        }

        throw std::runtime_error{"vkz logger is not installed"};
    }
}
