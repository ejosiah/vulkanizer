function(vulkanizer_add_test_executable target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE vulkanizer)
endfunction()

vulkanizer_add_test_executable(vulkanizer_context_test main.cpp)
vulkanizer_add_test_executable(vulkanizer_csm_test csm.cpp)
vulkanizer_add_test_executable(vulkanizer_primitive_test primitives.cpp)
vulkanizer_add_test_executable(vulkanizer_camera_test camera.cpp)

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

set(PRIMITIVE_TEST_SHADERS
        primitive.vert
        primitive.frag)

set(PRIMITIVE_TEST_SPV_FILES)
foreach(shader ${PRIMITIVE_TEST_SHADERS})
    set(source "${CSM_TEST_SHADER_SOURCE_DIR}/${shader}")
    set(output "${CSM_TEST_SHADER_BINARY_DIR}/${shader}.spv")
    add_custom_command(
            OUTPUT "${output}"
            COMMAND "${GLSLC_EXECUTABLE}" "${source}" -o "${output}"
            DEPENDS "${source}"
            VERBATIM)
    list(APPEND PRIMITIVE_TEST_SPV_FILES "${output}")
endforeach()

add_custom_target(vulkanizer_primitive_test_shaders DEPENDS ${PRIMITIVE_TEST_SPV_FILES})
add_dependencies(vulkanizer_primitive_test vulkanizer_primitive_test_shaders)
target_compile_definitions(vulkanizer_primitive_test PRIVATE VKZ_PRIMITIVE_TEST_SHADER_DIR="${CSM_TEST_SHADER_BINARY_DIR}")

set(CAMERA_TEST_SHADERS
        camera_floor.vert
        camera_floor.frag
        camera_skybox.vert
        camera_skybox.frag)
set(CAMERA_TEST_SPV_FILES)
foreach(shader ${CAMERA_TEST_SHADERS})
    set(source "${CSM_TEST_SHADER_SOURCE_DIR}/${shader}")
    set(output "${CSM_TEST_SHADER_BINARY_DIR}/${shader}.spv")
    add_custom_command(
            OUTPUT "${output}"
            COMMAND "${GLSLC_EXECUTABLE}" "${source}" -o "${output}"
            DEPENDS "${source}"
            VERBATIM)
    list(APPEND CAMERA_TEST_SPV_FILES "${output}")
endforeach()
add_custom_target(vulkanizer_camera_test_shaders DEPENDS ${CAMERA_TEST_SPV_FILES})
add_dependencies(vulkanizer_camera_test vulkanizer_camera_test_shaders)
target_compile_definitions(vulkanizer_camera_test PRIVATE
        VKZ_CAMERA_TEST_SHADER_DIR="${CSM_TEST_SHADER_BINARY_DIR}"
        VKZ_CAMERA_TEST_SKYBOX_DIR="${CMAKE_CURRENT_LIST_DIR}/resources/skybox")
if(WIN32)
    target_link_libraries(vulkanizer_camera_test PRIVATE windowscodecs ole32)
endif()
