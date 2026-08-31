#include "vulkanizer/io.hpp"

#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <vector>

vkz::byte_string vkz::loadFile(const std::string &path) {
    std::ifstream fin(path.data(), std::ios::binary | std::ios::ate);
    if(!fin.good()) throw std::runtime_error{"Failed to open file: " + path};

    auto size = fin.tellg();
    fin.seekg(0);
    std::vector<char> data(size);
    fin.read(data.data(), size);

    return data;
}
