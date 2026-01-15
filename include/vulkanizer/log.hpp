#pragma once

#include <string_view>

namespace vkz
{
    enum class LogLevel {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical
    };

    using LogFn = void(*)(LogLevel level, std::string_view message);

    void set_logger(LogFn fn);

    void log(LogLevel level, std::string_view message);

    inline void trace(std::string_view msg)    { log(LogLevel::Trace, msg); }
    inline void debug(std::string_view msg)    { log(LogLevel::Debug, msg); }
    inline void info(std::string_view msg)     { log(LogLevel::Info, msg); }
    inline void warn(std::string_view msg)     { log(LogLevel::Warn, msg); }
    inline void error(std::string_view msg)    { log(LogLevel::Error, msg); }
    inline void critical(std::string_view msg) { log(LogLevel::Critical, msg); }

#ifdef VKZ_DISABLE_LOGGING
    inline void log(LogLevel, std::string_view) {}
#endif
}


#ifdef VKZ_SPD_LOG_ADAPTOR
#include <spdlog/spdlog.h>
namespace vkz::spdlog_adapter {
    inline void log(LogLevel level, std::string_view msg) {
        switch (level)
        {
        case LogLevel::Trace:    spdlog::trace(msg); break;
        case LogLevel::Debug:    spdlog::debug(msg); break;
        case LogLevel::Info:     spdlog::info(msg); break;
        case LogLevel::Warn:     spdlog::warn(msg); break;
        case LogLevel::Error:    spdlog::error(msg); break;
        case LogLevel::Critical: spdlog::critical(msg); break;
        }
    }

    inline void install() {
        vkz::set_logger(&log);
    }
}
#endif

#ifdef VKZ_IOSTREAM_ADAPTOR
#include <ostream>
#include <mutex>

namespace vkz::iostream_adapter
{
    namespace detail
    {
        inline std::ostream* out = nullptr;
        inline std::mutex mutex;
    }

    inline const char* to_string(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warn:     return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        }
        return "UNKNOWN";
    }

    inline void log(LogLevel level, std::string_view msg)
    {
        if (!detail::out)
            return;

        std::lock_guard lock(detail::mutex);

        (*detail::out)
            << "[" << to_string(level) << "] "
            << msg
            << std::endl;
    }

    inline void install(std::ostream& stream)
    {
        detail::out = &stream;
        vkz::set_logger(&log);
    }
}

#endif