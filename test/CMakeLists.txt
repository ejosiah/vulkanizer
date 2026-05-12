add_library(vulkanizer_test_common STATIC vulkan_app.cpp vulkan_app.hpp)
target_link_libraries(vulkanizer_test_common PUBLIC vulkanizer glfw)

function(vulkanizer_add_test_executable target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE vulkanizer_test_common)
endfunction()

vulkanizer_add_test_executable(vulkanizer_context_test main.cpp)
vulkanizer_add_test_executable(vulkanizer_csm_test csm.cpp)

find_program(GLSLC_EXECUTABLE glslc)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc is required to build the CSM test shaders")
endif()

set(CSM_TEST_SHADER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders")
set(CSM_TEST_SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${CSM_TEST_SHADER_BINARY_DIR}")

set(CSM_TEST_SHADERS
        csm_scene.vert
        csm_scene.frag)

set(CSM_TEST_SPV_FILES)
foreach(shader ${CSM_TEST_SHADERS})
    set(source "${CSM_TEST_SHADER_SOURCE_DIR}/${shader}")
    set(output "${CSM_TEST_SHADER_BINARY_DIR}/${shader}.spv")
    add_custom_command(
            OUTPUT "${output}"
            COMMAND "${GLSLC_EXECUTABLE}" "${source}" -o "${output}"
            DEPENDS "${source}"
            VERBATIM)
    list(APPEND CSM_TEST_SPV_FILES "${output}")
endforeach()

add_custom_target(vulkanizer_csm_test_shaders DEPENDS ${CSM_TEST_SPV_FILES})
add_dependencies(vulkanizer_csm_test vulkanizer_csm_test_shaders)
target_compile_definitions(vulkanizer_csm_test PRIVATE VKZ_CSM_TEST_SHADER_DIR="${CSM_TEST_SHADER_BINARY_DIR}")
