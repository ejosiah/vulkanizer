#include "vulkanizer/profiler.hpp"

#include "vulkanizer/status.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace vkz {

    profiler::profiler(vkz::device device, uint32_t query_count) {
        init(device, query_count);
    }

    profiler::~profiler() {
        deinit();
    }

    profiler::profiler(profiler &&other) noexcept {
        *this = std::move(other);
    }

    profiler &profiler::operator=(profiler &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        deinit();
        external_reset = other.external_reset;
        paused = other.paused;
        queries = std::move(other.queries);
        cpu_queries = std::move(other.cpu_queries);
        query_pool_ = std::exchange(other.query_pool_, VK_NULL_HANDLE);
        device_ = std::exchange(other.device_, {});
        timestamp_period_ = std::exchange(other.timestamp_period_, 0.0f);
        query_capacity_ = std::exchange(other.query_capacity_, default_query_count);
        query_groups_ = std::move(other.query_groups_);
        return *this;
    }

    void profiler::init(vkz::device device, uint32_t query_count) {
        if (!device || !device.physical) {
            throw std::invalid_argument{"Valid physical and logical Vulkan devices are required to initialize a profiler"};
        }

        deinit();
        queries.clear();
        query_groups_.clear();
        device_ = device;
        query_capacity_ = std::max(query_count, 1u);

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device_.physical, &properties);
        timestamp_period_ = properties.limits.timestampPeriod;
        create_query_pool(query_capacity_);
    }

    void profiler::deinit() {
        if (query_pool_) {
            vkDestroyQueryPool(device_, query_pool_, nullptr);
            query_pool_ = VK_NULL_HANDLE;
        }

        device_ = {};
        timestamp_period_ = 0.0f;
    }

    void profiler::add_query(const std::string &name) {
        if (!is_ready()) {
            return;
        }

        if (queries.contains(name)) {
            throw std::invalid_argument{"Profiler query already exists: " + name};
        }

        if (queries.size() >= query_capacity_) {
            query_capacity_ = std::max(query_capacity_ + query_capacity_ / 4, query_capacity_ + 1);
            create_query_pool(query_capacity_);
        }

        const auto start_id = static_cast<uint32_t>(queries.size() * 2);
        queries.emplace(name, query{name, start_id, start_id + 1});
    }

    void profiler::group(const std::string &group_name, std::span<const std::string> query_names) {
        if (!is_ready()) {
            return;
        }

        auto &query_group = query_groups_[group_name];
        query_group.name = group_name;
        for (const auto &query_name : query_names) {
            if (!queries.contains(query_name)) {
                throw std::invalid_argument{"Unknown profiler query: " + query_name};
            }
            query_group.queries.push_back(query_name);
        }
    }

    void profiler::add_group(const std::string &name, uint32_t query_count) {
        if (!is_ready()) {
            return;
        }

        if (query_count == 0) {
            throw std::invalid_argument{"A profiler group must contain at least one query"};
        }

        std::vector<std::string> query_names;
        query_names.reserve(query_count);
        for (uint32_t index = 0; index < query_count; ++index) {
            auto query_name = name + "_" + std::to_string(index);
            add_query(query_name);
            query_names.push_back(std::move(query_name));
        }
        group(name, query_names);
    }

    void profiler::commit() {
        if (!is_ready() || queries.empty()) {
            return;
        }

        const auto timestamps = read_timestamps();
        for (auto &[name, query] : queries) {
            const auto elapsed = static_cast<uint64_t>(static_cast<double>(timestamps[query.end_id] - timestamps[query.start_id]) * timestamp_period_);
            query.runtimes.push_back(elapsed);
        }

        for (auto &[name, query_group] : query_groups_) {
            uint64_t elapsed{};
            for (const auto &query_name : query_group.queries) {
                elapsed += queries.at(query_name).runtimes.back();
            }
            query_group.runtimes.push_back(elapsed / query_group.queries.size());
        }
    }

    void profiler::commit(std::span<const std::string> query_names) {
        if (!is_ready()) {
            return;
        }
        for (const auto &name : query_names) {
            auto &query = queries.at(name);
            uint64_t timestamps[2]{};
            VKZ_CHECK_VULKAN(vkGetQueryPoolResults(device_, query_pool_, query.start_id, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT));
            query.runtimes.push_back(static_cast<uint64_t>(static_cast<double>(timestamps[1] - timestamps[0]) * timestamp_period_));
        }
    }

    void profiler::clear_runtimes() {
        for (auto &[name, query] : cpu_queries) {
            query.runtimes.clear();
        }
        for (auto &[name, query] : queries) {
            query.runtimes.clear();
        }

        for (auto &[name, query_group] : query_groups_) {
            query_group.runtimes.clear();
        }
    }

    void profiler::end_frame() {
        if (!is_ready() || queries.empty()) {
            return;
        }

        const auto timestamps = read_timestamps();
        for (auto &[name, query] : queries) {
            const auto elapsed = static_cast<float>(static_cast<double>(timestamps[query.end_id] - timestamps[query.start_id]) * timestamp_period_);
            const auto previous = query.average.value;
            if (previous != 0.0f && elapsed / previous > 3.0f) {
                continue;
            }

            const auto count = static_cast<float>(++query.average.count);
            query.average.value = previous + (elapsed - previous) / count;
        }
    }

    std::optional<profiler::query_group> profiler::get_group(const std::string &name) const {
        const auto iterator = query_groups_.find(name);
        if (iterator == query_groups_.end()) {
            return std::nullopt;
        }
        return iterator->second;
    }

    std::map<std::string, profiler::statistics> profiler::group_stats() const {
        std::map<std::string, statistics> result;
        for (const auto &[name, query_group] : query_groups_) {
            result.emplace(name, summarize(to_milliseconds(query_group.runtimes)));
        }
        return result;
    }

    std::map<std::string, profiler::statistics> profiler::query_stats() const {
        std::map<std::string, statistics> result;
        for (const auto &[name, query] : queries) {
            result.emplace(name, summarize(to_milliseconds(query.runtimes)));
        }
        return result;
    }

    void profiler::add_cpu_query(const std::string &name) {
        if (!cpu_queries.emplace(name, query{name}).second) {
            throw std::invalid_argument{"CPU profiler query already exists: " + name};
        }
    }

    std::map<std::string, profiler::statistics> profiler::cpu_query_stats() const {
        std::map<std::string, statistics> result;
        for (const auto &[name, query] : cpu_queries) {
            result.emplace(name, summarize(to_milliseconds(query.runtimes)));
        }
        return result;
    }

    void profiler::clear_cpu(const std::string &name) {
        const auto iterator = cpu_queries.find(name);
        if (iterator == cpu_queries.end()) {
            throw std::invalid_argument{"Unknown CPU profiler query: " + name};
        }
        iterator->second.average = {};
    }

    void profiler::reset_all(VkCommandBuffer command_buffer) const {
        if (is_ready() && !queries.empty()) {
            vkCmdResetQueryPool(command_buffer, query_pool_, 0, static_cast<uint32_t>(queries.size() * 2));
        }
    }

    void profiler::reset(const std::string &name, VkCommandBuffer command_buffer) const {
        const auto iterator = queries.find(name);
        if (iterator == queries.end()) {
            throw std::invalid_argument{"Unknown profiler query: " + name};
        }
        vkCmdResetQueryPool(command_buffer, query_pool_, iterator->second.start_id, 2);
    }

    void profiler::clear(const std::string &name) {
        const auto iterator = queries.find(name);
        if (iterator == queries.end()) {
            throw std::invalid_argument{"Unknown profiler query: " + name};
        }
        iterator->second.average = {};
    }

    std::vector<float> profiler::to_milliseconds(std::span<const uint64_t> durations) {
        std::vector<float> result;
        result.reserve(durations.size());
        for (const auto duration : durations) {
            result.push_back(to_milliseconds(duration));
        }
        return result;
    }

    bool profiler::is_ready() const {
        return query_pool_ != VK_NULL_HANDLE && device_ && !paused;
    }

    void profiler::create_query_pool(uint32_t query_count) {
        VkQueryPool replacement{};
        VkQueryPoolCreateInfo create_info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
        create_info.queryCount = query_count * 2;
        VKZ_CHECK_VULKAN(vkCreateQueryPool(device_, &create_info, nullptr, &replacement));

        if (query_pool_) {
            vkDestroyQueryPool(device_, query_pool_, nullptr);
        }
        query_pool_ = replacement;
    }

    std::vector<uint64_t> profiler::read_timestamps() const {
        std::vector<uint64_t> timestamps(queries.size() * 2);
        VKZ_CHECK_VULKAN(vkGetQueryPoolResults(device_, query_pool_, 0, static_cast<uint32_t>(timestamps.size()), timestamps.size() * sizeof(uint64_t), timestamps.data(), sizeof(uint64_t),
                                               VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
        return timestamps;
    }

    profiler::statistics profiler::summarize(std::vector<float> values) {
        if (values.empty()) {
            return {};
        }

        std::ranges::sort(values);
        const auto middle = values.size() / 2;
        const auto median = values.size() % 2 == 0 ? (values[middle - 1] + values[middle]) * 0.5f : values[middle];
        const auto mean = std::accumulate(values.begin(), values.end(), 0.0f) / static_cast<float>(values.size());
        const auto squared_difference = [mean](float sum, float value) {
            const auto difference = value - mean;
            return sum + difference * difference;
        };
        const auto variance = std::accumulate(values.begin(), values.end(), 0.0f, squared_difference) / static_cast<float>(values.size());
        return {values.front(), values.back(), median, mean, variance, std::sqrt(variance)};
    }

} // namespace vkz
