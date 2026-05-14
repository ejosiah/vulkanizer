cmake_minimum_required(VERSION 3.26)
project(vulkanizer VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 20)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(VULKANIZER_BUILD_TESTS "Build vulkanizer tests" ON)

find_package(volk REQUIRED)
find_package(glfw3 REQUIRED)
find_package(imgui REQUIRED)
find_package(glm)
find_package(VulkanMemoryAllocator)

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

set(INCLUDE_DIRS $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
set(INSTALL_INCLUDE_DIRS $<INSTALL_INTERFACE:include>)

add_definitions(
        -DGLM_FORCE_SWIZZLE
)


file(GLOB_RECURSE HPP_FILES ${CMAKE_CURRENT_LIST_DIR}/include/*.*)
file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
add_library(vulkanizer STATIC
        ${HPP_FILES}
        ${CPP_FILES}
        "${IMGUI_BINDINGS_DIR}/imgui_impl_glfw.cpp"
        "${IMGUI_BINDINGS_DIR}/imgui_impl_vulkan.cpp")
add_library(vulkanizer::vulkanizer ALIAS vulkanizer)
target_link_libraries(vulkanizer PUBLIC volk::volk glfw glm::glm GPUOpen::VulkanMemoryAllocator imgui::imgui)

target_include_directories(vulkanizer PUBLIC ${INCLUDE_DIRS} ${INSTALL_INCLUDE_DIRS})
target_include_directories(vulkanizer PRIVATE "${IMGUI_BINDINGS_DIR}")
target_compile_definitions(vulkanizer PRIVATE GLFW_INCLUDE_NONE IMGUI_IMPL_VULKAN_USE_VOLK)

install(TARGETS vulkanizer
        EXPORT vulkanizer_targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT vulkanizer_targets
        FILE vulkanizer-targets.cmake
        NAMESPACE vulkanizer::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vulkanizer)

configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/vulkanizer-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/vulkanizer-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vulkanizer)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/vulkanizer-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/vulkanizer-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/vulkanizer-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vulkanizer)

if(VULKANIZER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
