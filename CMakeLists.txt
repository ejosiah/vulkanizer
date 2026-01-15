cmake_minimum_required(VERSION 3.31)
project(vulkanizer)
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan)

include_directories(include)
set(INCLUDE_DIRS $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
set(INSTALL_INCLUDE_DIRS $<INSTALL_INTERFACE:include>)

file(GLOB_RECURSE HPP_FILES ${CMAKE_CURRENT_LIST_DIR}/include/*.*)
file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB_RECURSE TEST_FILES ${CMAKE_CURRENT_LIST_DIR}/tests/*.*)

add_library(vulkanizer STATIC ${HPP_FILES} ${CPP_FILES})
target_link_libraries(vulkanizer PRIVATE Vulkan::Vulkan)

target_include_directories(vulkanizer PUBLIC ${INCLUDE_DIRS} ${INSTALL_INCLUDE_DIRS})
