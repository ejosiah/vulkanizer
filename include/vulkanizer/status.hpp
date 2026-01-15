#pragma once

#include <cassert>
#include <stdexcept>
#include <format>


#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)
#define FILE_AND_LINE __FILE__ ":" LINE_STRING

#define VKZ_REPORT_ERROR(result, msg) if(result != VK_SUCCESS) throw std::runtime_error{msg};
#define VKZ_THROW(msg) vkz_throw(msg, nullptr, FILE_AND_LINE);

#define VKZ_CHECK_VULKAN(expr) do { if((expr) < 0) { \
        assert(0 && #expr); \
        throw std::runtime_error(__FILE__ "(" LINE_STRING "): VkResult( " #expr " ) < 0"); \
    } } while(false)

#define VKZ_COUNT(sequence) static_cast<uint32_t>(sequence.size())
#define VKZ_BYTE_SIZE(sequence) static_cast<VkDeviceSize>(sizeof(sequence[0]) * sequence.size())


[[noreturn]] inline void
vkz_throw(std::string failureMessage, const char *originator = nullptr, const char *sourceLocation = nullptr) {
    if (originator != nullptr) {
        failureMessage += std::format("\n    Origin: {}", originator);
    }
    if (sourceLocation != nullptr) {
        failureMessage += std::format("\n    Source: {}", sourceLocation);
    }

    throw std::runtime_error(failureMessage.c_str());
}

