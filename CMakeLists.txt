cmake_minimum_required(VERSION 3.31)
project(vulkanizer)
set(CMAKE_CXX_STANDARD 20)

find_package(volk REQUIRED)
find_package(glfw3 REQUIRED)
find_package(imgui REQUIRED)

set(IMGUI_BINDINGS_DIR "")
foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    if(DEFINED imgui_PACKAGE_FOLDER_${CONFIG_TYPE}
            AND EXISTS "${imgui_PACKAGE_FOLDER_${CONFIG_TYPE}}/res/bindings/imgui_impl_glfw.cpp"
            AND EXISTS "${imgui_PACKAGE_FOLDER_${CONFIG_TYPE}}/res/bindings/imgui_impl_vulkan.cpp")
        set(IMGUI_BINDINGS_DIR "${imgui_PACKAGE_FOLDER_${CONFIG_TYPE}}/res/bindings")
        break()
    endif()
endforeach()

if(NOT IMGUI_BINDINGS_DIR)
    message(FATAL_ERROR "Could not find Dear ImGui backend sources in the Conan imgui package")
endif()

include_directories(include)
set(INCLUDE_DIRS $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
set(INSTALL_INCLUDE_DIRS $<INSTALL_INTERFACE:include>)

file(GLOB_RECURSE HPP_FILES ${CMAKE_CURRENT_LIST_DIR}/include/*.*)
file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB_RECURSE TEST_FILES ${CMAKE_CURRENT_LIST_DIR}/test/*.*)

add_library(vulkanizer STATIC
        ${HPP_FILES}
        ${CPP_FILES}
        "${IMGUI_BINDINGS_DIR}/imgui_impl_glfw.cpp"
        "${IMGUI_BINDINGS_DIR}/imgui_impl_vulkan.cpp")
target_link_libraries(vulkanizer PUBLIC volk::volk imgui::imgui glfw)

target_include_directories(vulkanizer PUBLIC ${INCLUDE_DIRS} ${INSTALL_INCLUDE_DIRS})
target_include_directories(vulkanizer PRIVATE "${IMGUI_BINDINGS_DIR}")
target_compile_definitions(vulkanizer PRIVATE GLFW_INCLUDE_NONE IMGUI_IMPL_VULKAN_USE_VOLK)

add_executable(vulkanizer_test ${TEST_FILES})
target_link_libraries(vulkanizer_test PRIVATE vulkanizer glfw)
