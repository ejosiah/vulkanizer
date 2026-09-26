file(READ "${INPUT}" bytes HEX)
string(REGEX REPLACE "(..)(..)(..)(..)" "0x\\4\\3\\2\\1," words "${bytes}")
file(WRITE "${OUTPUT}" "#pragma once\n#include <cstdint>\ninline constexpr uint32_t ${SYMBOL}[] = {${words}};\n")
