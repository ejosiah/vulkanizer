#include "vulkanizer/aftermath.hpp"

#include "vulkanizer/log.hpp"

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>

#ifdef VKZ_ENABLE_NSIGHT_AFTERMATH
#include <GFSDK_Aftermath_GpuCrashDump.h>
#endif

namespace vkz::aftermath {
#ifdef VKZ_ENABLE_NSIGHT_AFTERMATH
    namespace {
        std::mutex state_mutex;
        std::filesystem::path dump_directory{"aftermath"};
        std::atomic_uint64_t next_file_id{};
        bool enabled{};

        void write_blob(const void* data, uint32_t size, const char* extension) noexcept {
            try {
                std::lock_guard lock{state_mutex};
                std::filesystem::create_directories(dump_directory);
                const auto id = next_file_id.fetch_add(1, std::memory_order_relaxed);
                const auto path = dump_directory / (std::to_string(id) + extension);
                std::ofstream output{path, std::ios::binary};
                output.write(static_cast<const char*>(data), size);
                output.flush();
            } catch (...) {
                // Exceptions must never escape an Aftermath callback.
            }
        }

        void GFSDK_AFTERMATH_CALL gpu_crash_dump_callback(
            const void* data, uint32_t size, void*) {
            write_blob(data, size, ".nv-gpudmp");
        }

        void GFSDK_AFTERMATH_CALL shader_debug_info_callback(
            const void* data, uint32_t size, void*) {
            write_blob(data, size, ".nvdbg");
        }

        void GFSDK_AFTERMATH_CALL description_callback(
            PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_value, void*) {
            add_value(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "vulkanizer");
            add_value(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "0.0.10");
        }
    }

    bool available() {
        return true;
    }

    bool enable(const std::filesystem::path& output_directory) {
        std::lock_guard lock{state_mutex};
        if (enabled) {
            return true;
        }

        dump_directory = output_directory;
        std::filesystem::create_directories(dump_directory);
        const auto result = GFSDK_Aftermath_EnableGpuCrashDumps(
            GFSDK_Aftermath_Version_API,
            GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
            GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,
            gpu_crash_dump_callback,
            shader_debug_info_callback,
            description_callback,
            nullptr,
            nullptr);
        enabled = GFSDK_Aftermath_SUCCEED(result);
        if (!enabled) {
            error("Failed to enable NVIDIA Nsight Aftermath GPU crash dumps");
        }
        return enabled;
    }

    void disable() {
        std::lock_guard lock{state_mutex};
        if (enabled) {
            GFSDK_Aftermath_DisableGpuCrashDumps();
            enabled = false;
        }
    }
#else
    bool available() {
        return false;
    }

    bool enable(const std::filesystem::path&) {
        return false;
    }

    void disable() {
    }
#endif
}
