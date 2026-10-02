cmake_minimum_required(VERSION 3.24)
project(vulkanizer VERSION 0.0.42)
set(CMAKE_CXX_STANDARD 20)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(VULKANIZER_BUILD_TESTS "Build vulkanizer tests" ON)
option(VULKANIZER_ENABLE_NSIGHT_AFTERMATH "Enable NVIDIA Nsight Aftermath GPU crash dumps" OFF)
set(NSIGHT_AFTERMATH_SDK_DIR "" CACHE PATH "Path to the NVIDIA Nsight Aftermath SDK")

# CLion may configure its build directory directly instead of using Conan's
# generated preset. In that case, make CMakeDeps packages discoverable from
# the Conan output nested beneath the active build directory.
set(VULKANIZER_CONAN_GENERATORS "${CMAKE_BINARY_DIR}/build/generators")
if(EXISTS "${VULKANIZER_CONAN_GENERATORS}/volk-config.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${VULKANIZER_CONAN_GENERATORS}")

    # This directory contains dependencies installed for Debug only. Limit a
    # multi-config generator so it does not request nonexistent Release libs.
    if(CMAKE_CONFIGURATION_TYPES
            AND EXISTS "${VULKANIZER_CONAN_GENERATORS}/volk-debug-x86_64-data.cmake"
            AND NOT EXISTS "${VULKANIZER_CONAN_GENERATORS}/volk-release-x86_64-data.cmake")
        set(CMAKE_CONFIGURATION_TYPES Debug CACHE STRING "Available configurations" FORCE)
    endif()
endif()

find_package(volk REQUIRED)
find_package(glfw3 REQUIRED)
find_package(imgui REQUIRED)
find_package(glm)
find_package(VulkanMemoryAllocator)
find_package(glslang REQUIRED)
find_package(Ktx REQUIRED)
find_package(stb REQUIRED)
find_package(TinyGLTF REQUIRED)

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
file(GLOB_RECURSE CPP_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
add_library(vulkanizer STATIC
        ${HPP_FILES}
        ${CPP_FILES}
        "${IMGUI_BINDINGS_DIR}/imgui_impl_glfw.cpp"
        "${IMGUI_BINDINGS_DIR}/imgui_impl_vulkan.cpp")
add_library(vulkanizer::vulkanizer ALIAS vulkanizer)

# Embed TAA SPIR-V so installed consumers need neither source paths nor runtime shader files.
find_program(VULKANIZER_GLSLC glslc REQUIRED)
set(TAA_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/taa")
set(TAA_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/taa")
file(MAKE_DIRECTORY "${TAA_GENERATED_DIR}")
foreach(shader IN ITEMS camera_motion resolve resolve16)
    set(source "${TAA_SHADER_DIR}/${shader}.comp")
    set(defines)
    if(shader STREQUAL "resolve16")
        set(source "${TAA_SHADER_DIR}/resolve.comp")
        set(defines -DTAA_RGBA16F)
    endif()
    set(spv "${TAA_GENERATED_DIR}/${shader}.spv")
    set(header "${TAA_GENERATED_DIR}/${shader}.hpp")
    add_custom_command(OUTPUT "${header}"
        COMMAND "${VULKANIZER_GLSLC}" --target-env=vulkan1.1 ${defines} "${source}" -o "${spv}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${spv}" "-DOUTPUT=${header}" "-DSYMBOL=taa_${shader}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        DEPENDS "${source}" "${TAA_SHADER_DIR}/uniforms.glsl" "${TAA_SHADER_DIR}/resolve.glsl" "${TAA_SHADER_DIR}/filters.glsl" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        VERBATIM)
    target_sources(vulkanizer PRIVATE "${header}")
endforeach()
target_include_directories(vulkanizer PRIVATE "${TAA_GENERATED_DIR}")

# Software-paged virtual shadows: internal SPIR-V is embedded; the public GLSL integration
# include is installed with the headers for application-owned deformation/material pipelines.
set(SVSM_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/svsm")
file(MAKE_DIRECTORY "${SVSM_GENERATED_DIR}")
foreach(shader IN ITEMS manage.comp shadow.vert shadow.frag)
    string(REPLACE "." "_" symbol "svsm_${shader}")
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/src/svsm/${shader}")
    set(header "${SVSM_GENERATED_DIR}/${symbol}.hpp")
    set(spv "${SVSM_GENERATED_DIR}/${shader}.spv")
    add_custom_command(OUTPUT "${header}"
        COMMAND "${VULKANIZER_GLSLC}" --target-env=vulkan1.3 -I "${CMAKE_CURRENT_SOURCE_DIR}/include" "${source}" -o "${spv}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${spv}" "-DOUTPUT=${header}" "-DSYMBOL=${symbol}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        DEPENDS "${source}" "${CMAKE_CURRENT_SOURCE_DIR}/include/vulkanizer/shaders/svsm.glsl" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        VERBATIM)
    target_sources(vulkanizer PRIVATE "${header}")
endforeach()
target_include_directories(vulkanizer PRIVATE "${SVSM_GENERATED_DIR}")

# Core glTF reference shaders are library resources, including tests-disabled builds.
# CMakeRC 2.0.1; vendored license is cmake/CMakeRC.LICENSE.txt.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeRC.cmake")
set(REFERENCE_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/reference")
set(REFERENCE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/reference")
file(MAKE_DIRECTORY "${REFERENCE_GENERATED_DIR}")
set(REFERENCE_SPV_FILES)
foreach(shader IN ITEMS gltf_reference.vert gltf_reference.frag gltf_reference.rgen gltf_reference.rmiss gltf_reference.rchit gltf_reference.rahit gltf_background.vert gltf_background.frag tone_mapping.frag)
    set(output "${REFERENCE_GENERATED_DIR}/${shader}.spv")
    set(reference_target_env vulkan1.0)
    if(NOT shader MATCHES "[.](vert|frag)$")
        set(reference_target_env vulkan1.3)
    endif()
    add_custom_command(OUTPUT "${output}"
        COMMAND "${VULKANIZER_GLSLC}" --target-env=${reference_target_env} "${REFERENCE_SHADER_DIR}/${shader}" -o "${output}"
        DEPENDS "${REFERENCE_SHADER_DIR}/${shader}"
            "${REFERENCE_SHADER_DIR}/gltf_reference_uniforms.glsl" "${REFERENCE_SHADER_DIR}/gltf_core.glsl"
            "${REFERENCE_SHADER_DIR}/gltf_types.glsl" "${REFERENCE_SHADER_DIR}/gltf_material.glsl" "${REFERENCE_SHADER_DIR}/gltf_iridescence.glsl" "${REFERENCE_SHADER_DIR}/gltf_layers.glsl" "${REFERENCE_SHADER_DIR}/gltf_ibl.glsl"
            "${REFERENCE_SHADER_DIR}/gltf_ray_scene.glsl" "${REFERENCE_SHADER_DIR}/gltf_ray_surface.glsl"
            "${REFERENCE_SHADER_DIR}/gltf_surface.glsl" "${REFERENCE_SHADER_DIR}/gltf_transport.glsl"
        VERBATIM)
    list(APPEND REFERENCE_SPV_FILES "${output}")
endforeach()
foreach(shader IN ITEMS gltf_reference.frag gltf_reference.rmiss gltf_reference.rchit)
    set(output "${REFERENCE_GENERATED_DIR}/${shader}.spv.ibl")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${VULKANIZER_GLSLC}" --target-env=vulkan1.3 -DVKZ_REFERENCE_IBL "${REFERENCE_SHADER_DIR}/${shader}" -o "${output}"
        DEPENDS "${REFERENCE_SHADER_DIR}/${shader}" "${REFERENCE_SHADER_DIR}/gltf_ibl.glsl"
            "${REFERENCE_SHADER_DIR}/gltf_reference_uniforms.glsl" "${REFERENCE_SHADER_DIR}/gltf_core.glsl"
            "${REFERENCE_SHADER_DIR}/gltf_types.glsl" "${REFERENCE_SHADER_DIR}/gltf_material.glsl" "${REFERENCE_SHADER_DIR}/gltf_iridescence.glsl" "${REFERENCE_SHADER_DIR}/gltf_layers.glsl"
            "${REFERENCE_SHADER_DIR}/gltf_ray_scene.glsl" "${REFERENCE_SHADER_DIR}/gltf_ray_surface.glsl"
            "${REFERENCE_SHADER_DIR}/gltf_surface.glsl" "${REFERENCE_SHADER_DIR}/gltf_transport.glsl"
        VERBATIM)
    list(APPEND REFERENCE_SPV_FILES "${output}")
endforeach()
foreach(ibl IN ITEMS OFF ON)
    set(output "${REFERENCE_GENERATED_DIR}/gltf_reference.frag.spv")
    set(query_defines -DVKZ_REFERENCE_RAY_QUERY)
    if(ibl)
        string(APPEND output ".ibl")
        list(APPEND query_defines -DVKZ_REFERENCE_IBL)
    endif()
    string(APPEND output ".query")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${VULKANIZER_GLSLC}" --target-env=vulkan1.3 ${query_defines} "${REFERENCE_SHADER_DIR}/gltf_reference.frag" -o "${output}"
        DEPENDS "${REFERENCE_SHADER_DIR}/gltf_reference.frag" "${REFERENCE_SHADER_DIR}/gltf_ray_query.glsl"
            "${REFERENCE_SHADER_DIR}/gltf_reference_uniforms.glsl" "${REFERENCE_SHADER_DIR}/gltf_core.glsl"
            "${REFERENCE_SHADER_DIR}/gltf_types.glsl" "${REFERENCE_SHADER_DIR}/gltf_material.glsl" "${REFERENCE_SHADER_DIR}/gltf_iridescence.glsl" "${REFERENCE_SHADER_DIR}/gltf_layers.glsl" "${REFERENCE_SHADER_DIR}/gltf_ibl.glsl"
            "${REFERENCE_SHADER_DIR}/gltf_surface.glsl" "${REFERENCE_SHADER_DIR}/gltf_transport.glsl"
        VERBATIM)
    list(APPEND REFERENCE_SPV_FILES "${output}")
endforeach()
cmrc_add_resource_library(vulkanizer_reference_resources NAMESPACE vkz_reference_shaders
    WHENCE "${REFERENCE_GENERATED_DIR}" ${REFERENCE_SPV_FILES})
# Compile cmrc's generated source into our archive, avoiding another library for
# CMake/Conan consumers to link or package. Its helper target is generation-only.
get_target_property(REFERENCE_RESOURCE_SOURCES vulkanizer_reference_resources SOURCES)
set_target_properties(vulkanizer_reference_resources PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(vulkanizer PRIVATE ${REFERENCE_RESOURCE_SOURCES})
set(IBL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/ibl")
file(MAKE_DIRECTORY "${IBL_GENERATED_DIR}")
set(IBL_RESOURCE_FILES)
foreach(shader IN ITEMS equirectangular irradiance specular)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/src/ibl/${shader}.comp")
    set(output "${IBL_GENERATED_DIR}/${shader}.comp.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${VULKANIZER_GLSLC}" --target-env=vulkan1.3 "${source}" -o "${output}"
        DEPENDS "${source}" "${CMAKE_CURRENT_SOURCE_DIR}/src/ibl/common.glsl"
        VERBATIM)
    list(APPEND IBL_RESOURCE_FILES "${output}")
endforeach()
configure_file("src/ibl/brdf_lut.ktx" "${IBL_GENERATED_DIR}/brdf_lut.ktx" COPYONLY)
list(APPEND IBL_RESOURCE_FILES "${IBL_GENERATED_DIR}/brdf_lut.ktx")
cmrc_add_resource_library(vulkanizer_ibl_resources NAMESPACE vkz_ibl
    WHENCE "${IBL_GENERATED_DIR}" ${IBL_RESOURCE_FILES})
get_target_property(IBL_RESOURCE_SOURCES vulkanizer_ibl_resources SOURCES)
set_target_properties(vulkanizer_ibl_resources PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(vulkanizer PRIVATE ${IBL_RESOURCE_SOURCES})

target_link_libraries(vulkanizer PRIVATE $<BUILD_INTERFACE:cmrc::base>)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeRC.LICENSE.txt"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/vulkanizer")

target_link_libraries(vulkanizer PUBLIC
        volk::volk
        glfw
        glm::glm
        GPUOpen::VulkanMemoryAllocator
        imgui::imgui
        KTX::ktx
        stb::stb
        glslang::glslang
        glslang::SPIRV)

target_include_directories(vulkanizer PUBLIC ${INCLUDE_DIRS} ${INSTALL_INCLUDE_DIRS})
target_link_libraries(vulkanizer PRIVATE TinyGLTF::TinyGLTF)
target_include_directories(vulkanizer PRIVATE "${IMGUI_BINDINGS_DIR}")
target_compile_definitions(vulkanizer PRIVATE GLFW_INCLUDE_NONE IMGUI_IMPL_VULKAN_USE_VOLK)

if(VULKANIZER_ENABLE_NSIGHT_AFTERMATH)
    if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The configured Nsight Aftermath SDK integration requires 64-bit Windows")
    endif()
    if(NOT EXISTS "${NSIGHT_AFTERMATH_SDK_DIR}/include/GFSDK_Aftermath_GpuCrashDump.h"
            OR NOT EXISTS "${NSIGHT_AFTERMATH_SDK_DIR}/lib/x64/GFSDK_Aftermath_Lib.x64.lib"
            OR NOT EXISTS "${NSIGHT_AFTERMATH_SDK_DIR}/lib/x64/GFSDK_Aftermath_Lib.x64.dll")
        message(FATAL_ERROR "NSIGHT_AFTERMATH_SDK_DIR does not point to a complete Nsight Aftermath SDK")
    endif()

    add_library(nsight_aftermath SHARED IMPORTED GLOBAL)
    set_target_properties(nsight_aftermath PROPERTIES
            IMPORTED_IMPLIB "${NSIGHT_AFTERMATH_SDK_DIR}/lib/x64/GFSDK_Aftermath_Lib.x64.lib"
            IMPORTED_LOCATION "${NSIGHT_AFTERMATH_SDK_DIR}/lib/x64/GFSDK_Aftermath_Lib.x64.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${NSIGHT_AFTERMATH_SDK_DIR}/include")
    target_link_libraries(vulkanizer PRIVATE nsight_aftermath)
    target_compile_definitions(vulkanizer PRIVATE VKZ_ENABLE_NSIGHT_AFTERMATH)
    install(FILES "${NSIGHT_AFTERMATH_SDK_DIR}/lib/x64/GFSDK_Aftermath_Lib.x64.dll"
            DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES "${NSIGHT_AFTERMATH_SDK_DIR}/lib/x64/GFSDK_Aftermath_Lib.x64.lib"
            DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

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

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/glTF-Sample-Renderer.txt" DESTINATION "${CMAKE_INSTALL_DATADIR}/vulkanizer/licenses")
