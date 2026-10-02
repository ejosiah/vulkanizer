#pragma once

#include <vulkanizer/mesh.hpp>
#include <cmath>
#include <stdexcept>

namespace vkz::mesh::detail {
    inline void validate_light(const light& value) {
        auto require = [](bool valid, const char* message) {
            if (!valid) throw std::runtime_error(message);
        };
        require(value.type == light_type::directional || value.type == light_type::point || value.type == light_type::spot,
                "Invalid punctual light type");
        for (int c = 0; c < 3; ++c) {
            require(std::isfinite(value.color[c]) && value.color[c] >= 0 && value.color[c] <= 1, "Invalid light color");
            require(std::isfinite(value.position[c]) && std::isfinite(value.direction[c]), "Invalid light placement");
        }
        require(std::isfinite(value.intensity) && value.intensity >= 0, "Invalid light intensity");
        require(std::isfinite(value.range) && value.range >= 0, "Invalid light range");
        if (value.type != light_type::point)
            require(std::abs(glm::dot(value.direction, value.direction) - 1.0f) <= 0.0001f, "Light direction must be normalized");
        require(std::isfinite(value.inner_cone_angle) && std::isfinite(value.outer_cone_angle), "Invalid light cone angles");
        if (value.type == light_type::spot)
            require(value.inner_cone_angle >= 0 && value.inner_cone_angle < value.outer_cone_angle
                    && value.outer_cone_angle <= std::numbers::pi_v<float> / 2.0f, "Invalid spotlight cone angles");
    }
}
