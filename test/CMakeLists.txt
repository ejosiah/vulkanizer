function(vulkanizer_add_test_executable target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE vulkanizer)
    add_test(NAME ${target} COMMAND ${target} ${ARGN})
    if(TARGET nsight_aftermath)
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:nsight_aftermath> $<TARGET_FILE_DIR:${target}>)
    endif()
endfunction()

vulkanizer_add_test_executable(vulkanizer_context_test main.cpp --timeout 2)
vulkanizer_add_test_executable(vulkanizer_csm_test csm.cpp --timeout 2)
vulkanizer_add_test_executable(vulkanizer_primitive_test primitives.cpp --timeout 2)
vulkanizer_add_test_executable(vulkanizer_camera_test camera.cpp --timeout 2 --no-game-controller)
vulkanizer_add_test_executable(vulkanizer_camera_movement_test camera_movement.cpp)
vulkanizer_add_test_executable(vulkanizer_commands_test commands.cpp)
vulkanizer_add_test_executable(vulkanizer_profiler_test profiler.cpp)
vulkanizer_add_test_executable(vulkanizer_mip_map_test mip_map.cpp)
vulkanizer_add_test_executable(vulkanizer_texture_test texture.cpp "${CMAKE_CURRENT_LIST_DIR}/resources/skybox/front.jpg" --timeout 2)

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

vulkanizer_add_test_executable(vulkanizer_taa_test taa.cpp)

vulkanizer_add_test_executable(vulkanizer_svsm_test svsm.cpp)
set(SVSM_TEST_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/svsm_shaders")
file(MAKE_DIRECTORY "${SVSM_TEST_SHADER_DIR}")
add_custom_command(OUTPUT "${SVSM_TEST_SHADER_DIR}/svsm_sample.comp.spv"
    COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.3 -I "${PROJECT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/svsm_sample.comp" -o "${SVSM_TEST_SHADER_DIR}/svsm_sample.comp.spv"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/svsm_sample.comp" "${PROJECT_SOURCE_DIR}/include/vulkanizer/shaders/svsm.glsl"
    VERBATIM)
add_custom_target(vulkanizer_svsm_test_shaders DEPENDS "${SVSM_TEST_SHADER_DIR}/svsm_sample.comp.spv")
add_dependencies(vulkanizer_svsm_test vulkanizer_svsm_test_shaders)
target_compile_definitions(vulkanizer_svsm_test PRIVATE VKZ_SVSM_SHADER_DIR="${SVSM_TEST_SHADER_DIR}")

# Large local assets are optional; build the viewer everywhere, run it explicitly with --obj.
add_executable(vulkanizer_svsm_bistro svsm_bistro.cpp)
target_link_libraries(vulkanizer_svsm_bistro PRIVATE vulkanizer)
target_compile_definitions(vulkanizer_svsm_bistro PRIVATE
    VKZ_SVSM_SHADER_DIR="${SVSM_TEST_SHADER_DIR}" VKZ_SVSM_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
set(BISTRO_SHADER_OUTPUTS)
foreach(shader IN ITEMS bistro.vert bistro.frag bistro_depth.frag bistro_shadow.vert bistro_shadow.frag bistro_sky.vert bistro_sky.frag bistro_pages.frag)
    set(output "${SVSM_TEST_SHADER_DIR}/${shader}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.3 -I "${PROJECT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" -o "${output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/bistro_common.glsl"
            "${PROJECT_SOURCE_DIR}/include/vulkanizer/shaders/svsm.glsl"
        VERBATIM)
    list(APPEND BISTRO_SHADER_OUTPUTS "${output}")
endforeach()
add_custom_target(vulkanizer_bistro_shaders DEPENDS ${BISTRO_SHADER_OUTPUTS})
add_dependencies(vulkanizer_svsm_bistro vulkanizer_bistro_shaders)
