#include "vulkanizer/log.hpp"

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
        }
        // else: intentionally silent (library must never decide policy)
    }
}
