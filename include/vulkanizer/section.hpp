#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include <random>
#include <string>

namespace vkz {

    class section {
    public:
        section(VkCommandBuffer command_buffer, const std::string& name, const glm::vec4 color = random_color())
        : command_buffer_(command_buffer)
        {
#ifndef NDEBUG
            VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
            label.pLabelName = name.c_str();

            label.color[0] = color.r;
            label.color[1] = color.g;
            label.color[2] = color.b;
            label.color[3] = color.a;

            vkCmdBeginDebugUtilsLabelEXT(command_buffer_, &label);
#endif
        }

        ~section() {
#ifndef NDEBUG
            vkCmdEndDebugUtilsLabelEXT(command_buffer_);
#endif
        }

    private:
        static glm::vec4 random_color() {
            thread_local std::mt19937 generator{std::random_device{}()};
            thread_local std::uniform_real_distribution<float> distribution{0.0f, 1.0f};
            return {distribution(generator), distribution(generator), distribution(generator), 1.0f};
        }

        VkCommandBuffer command_buffer_{};
    };

}
