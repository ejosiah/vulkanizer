#include <vulkanizer/profiler.hpp>

#include <cmath>
#include <iostream>
#include <thread>

void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error{message};
    }
}

int main() {
    try {
        vkz::profiler profiler;
        profiler.add_cpu_query("work");
        bool executed = false;
        profiler.profile_cpu("work", [&] {
            executed = true;
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        });
        require(executed && !profiler.is_ready(), "CPU timing must work without Vulkan initialization");
        require(profiler.cpu_queries.at("work").runtimes.size() == 1, "Missing CPU sample");
        require(profiler.cpu_query_stats().at("work").minimum >= 1.0f, "CPU timing must report milliseconds");
        profiler.profile_cpu("work", [] {
        });
        require(profiler.cpu_queries.at("work").average.count == 2, "Repeated samples must update the average");
        profiler.paused = true;
        executed = false;
        profiler.profile_cpu("work", [&] {
            executed = true;
        });
        require(executed && profiler.cpu_queries.at("work").runtimes.size() == 2, "Pause must execute without recording");
        profiler.paused = false;
        bool threw = false;
        try {
            profiler.profile_cpu("work", [] {
                throw std::runtime_error{"body failure"};
            });
        } catch (const std::runtime_error &) {
            threw = true;
        }
        require(threw && profiler.cpu_queries.at("work").runtimes.size() == 2, "Exceptions must propagate without adding a sample");
        threw = false;
        try {
            profiler.add_cpu_query("work");
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        require(threw, "Duplicate query must be rejected");
        threw = false;
        try {
            profiler.profile_cpu("unknown", [] {
            });
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        require(threw, "Unknown query must be rejected");
        // Known samples independently verify statistics and unit conversion.
        profiler.cpu_queries.at("work").runtimes = {1000000, 3000000, 2000000};
        const auto stats = profiler.cpu_query_stats().at("work");
        require(stats.minimum == 1 && stats.maximum == 3 && stats.median == 2 && stats.mean == 2, "Incorrect statistics");
        require(std::abs(stats.variance - 2.0f / 3.0f) < 0.0001f, "Incorrect variance");
        vkz::profiler moved{std::move(profiler)};
        require(moved.cpu_queries.at("work").runtimes.size() == 3, "Move must preserve CPU samples");
        moved.commit();
        moved.end_frame();
        require(moved.cpu_queries.at("work").runtimes.size() == 3, "GPU frame calls must not duplicate CPU samples");
        moved.clear_cpu("work");
        require(moved.cpu_queries.at("work").average.count == 0, "Clear must reset CPU average");
        moved.clear_runtimes();
        require(moved.cpu_queries.at("work").runtimes.empty(), "Clear runtimes must include CPU samples");
        std::cout << "CPU profiler tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
