#define VKZ_IOSTREAM_ADAPTOR
#include <vulkanizer/log.hpp>
#include <iostream>

int main()
{
    vkz::iostream_adapter::install(std::cout);

    vkz::info("Hello world");
    vkz::warn("Something looks off");

    return 0;
}
