#pragma once

#include "vkz.hpp"

#include <volk.h>

#include <cstdint>
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vkz {

    class profiler {
      public:
        struct moving_average {
            float value{};
            uint32_t count{};
        };

        struct query {
            std::string name;
            uint32_t start_id{};
            uint32_t end_id{};
            std::vector<uint64_t> runtimes;
            moving_average average;
        };

        struct query_group {
            std::string name;
            std::vector<std::string> queries;
            std::vector<uint64_t> runtimes;
        };

        struct statistics {
            float minimum{};
            float maximum{};
            float median{};
            float mean{};
            float variance{};
            float standard_deviation{};
        };

        static constexpr uint32_t default_query_count = 1024;

        profiler() = default;

        explicit profiler(vkz::device device, uint32_t query_count = default_query_count);

        ~profiler();

        profiler(const profiler &) = delete;

        profiler &operator=(const profiler &) = delete;

        profiler(profiler &&other) noexcept;

        profiler &operator=(profiler &&other) noexcept;

        void init(vkz::device device, uint32_t query_count = default_query_count);

        void deinit();

        void add_query(const std::string &name);

        // CPU queries measure elapsed wall time, including waits, using a monotonic clock.
        // They require no Vulkan device. Like GPU queries, access must be externally synchronized.
        void add_cpu_query(const std::string &name);

        // Completed calls record nanosecond samples and update the average immediately.
        // No commit/end_frame is needed. Paused calls still execute; throwing calls record no sample.
        template <typename Body> void profile_cpu(const std::string &name, Body &&body) {
            if (paused) {
                std::invoke(std::forward<Body>(body));
                return;
            }
            const auto iterator = cpu_queries.find(name);
            if (iterator == cpu_queries.end()) {
                throw std::invalid_argument{"Unknown CPU profiler query: " + name};
            }
            const auto start = std::chrono::steady_clock::now();
            std::invoke(std::forward<Body>(body));
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
            auto &sample = iterator->second;
            sample.runtimes.push_back(static_cast<uint64_t>(elapsed));
            sample.average.value += (static_cast<float>(elapsed) - sample.average.value) / static_cast<float>(++sample.average.count);
        }

        // Statistics use milliseconds (variance uses milliseconds squared).
        [[nodiscard]] std::map<std::string, statistics> cpu_query_stats() const;

        void clear_cpu(const std::string &name);

        void group(const std::string &group_name, std::span<const std::string> query_names);

        void add_group(const std::string &name, uint32_t query_count);

        template <typename Body> void profile(const std::string &name, VkCommandBuffer command_buffer, Body &&body, VkPipelineStageFlagBits stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
            if (!is_ready()) {
                std::invoke(std::forward<Body>(body));
                return;
            }

            const auto iterator = queries.find(name);
            if (iterator == queries.end()) {
                throw std::invalid_argument{"Unknown profiler query: " + name};
            }

            const auto &query = iterator->second;
            if (!external_reset) {
                vkCmdResetQueryPool(command_buffer, query_pool_, query.start_id, 2);
            }

            vkCmdWriteTimestamp(command_buffer, stage, query_pool_, query.start_id);
            std::invoke(std::forward<Body>(body));
            vkCmdWriteTimestamp(command_buffer, stage, query_pool_, query.end_id);
        }

        void commit();

        // Collect only queries recorded in this submission. The caller must ensure
        // completion before reading (for example, by checking its submission fence).
        void commit(std::span<const std::string> query_names);

        void clear_runtimes();

        void end_frame();

        [[nodiscard]] std::optional<query_group> get_group(const std::string &name) const;

        [[nodiscard]] std::map<std::string, statistics> group_stats() const;

        [[nodiscard]] std::map<std::string, statistics> query_stats() const;

        void reset_all(VkCommandBuffer command_buffer) const;

        void reset(const std::string &name, VkCommandBuffer command_buffer) const;

        void clear(const std::string &name);

        [[nodiscard]] static constexpr float to_milliseconds(uint64_t duration) {
            return static_cast<float>(duration) * 1e-6f;
        }

        [[nodiscard]] static std::vector<float> to_milliseconds(std::span<const uint64_t> durations);

        [[nodiscard]] bool is_ready() const;

        bool external_reset{};
        bool paused{};
        std::map<std::string, query> queries;
        std::map<std::string, query> cpu_queries;

      private:
        void create_query_pool(uint32_t query_count);

        [[nodiscard]] std::vector<uint64_t> read_timestamps() const;

        [[nodiscard]] static statistics summarize(std::vector<float> values);

        VkQueryPool query_pool_{VK_NULL_HANDLE};
        vkz::device device_{};
        float timestamp_period_{};
        uint32_t query_capacity_{default_query_count};
        std::map<std::string, query_group> query_groups_;
    };

} // namespace vkz
