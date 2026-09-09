#pragma once

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>

class test_timeout {
public:
    test_timeout(int argc, char **argv) {
        for (int index = 1; index < argc; ++index) {
            if (std::string_view{argv[index]} == "--no-game-controller") {
                game_controller_enabled_ = false;
                continue;
            }
            if (std::string_view{argv[index]} != "--timeout") {
                continue;
            }
            if (++index >= argc) {
                throw std::invalid_argument{"--timeout requires a duration in seconds"};
            }
            const auto seconds = std::stof(argv[index]);
            deadline_ = clock::now() + std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>{seconds});
        }
    }

    [[nodiscard]] bool expired() const {
        return clock::now() >= deadline_;
    }

    [[nodiscard]] bool game_controller_enabled() const {
        return game_controller_enabled_;
    }

private:
    using clock = std::chrono::steady_clock;

    clock::time_point deadline_{clock::time_point::max()};
    bool game_controller_enabled_{true};
};
