#pragma once
#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
CMRC_DECLARE(sponza_shaders);
namespace reference {
inline std::vector<uint32_t> shader(const char* name) {
    const auto file = cmrc::sponza_shaders::get_filesystem().open(name);
    if (file.size() % 4) throw std::runtime_error("Invalid embedded SPIR-V size");
    std::vector<uint32_t> words(file.size() / 4);
    std::memcpy(words.data(), file.begin(), file.size());
    return words;
}
}
