set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
set(MESH_TEST_SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${MESH_TEST_SHADER_BINARY_DIR}")

vulkanizer_add_test_executable(vulkanizer_mesh_test mesh.cpp)
vulkanizer_add_test_executable(vulkanizer_mesh_convert_test mesh_convert.cpp)
target_link_libraries(vulkanizer_mesh_convert_test PRIVATE TinyGLTF::TinyGLTF)
find_program(VULKANIZER_KTX_TOOL NAMES ktx HINTS "${PROJECT_SOURCE_DIR}/build/tools/ktx/bin")
if(VULKANIZER_KTX_TOOL)
    set_tests_properties(vulkanizer_mesh_convert_test PROPERTIES ENVIRONMENT "VKZ_KTX_EXECUTABLE=${VULKANIZER_KTX_TOOL}")
endif()

# Incremental rendering test using the locally converted Sponza scene.
add_executable(vulkanizer_sponza_test sponza.cpp)
target_link_libraries(vulkanizer_sponza_test PRIVATE vulkanizer)
target_compile_definitions(vulkanizer_sponza_test PRIVATE
    VKZ_SPONZA_MESH="${CMAKE_CURRENT_SOURCE_DIR}/resources/sponza/Sponza.vkmesh")
set(SPONZA_SHADER_OUTPUTS)
foreach(shader IN ITEMS sponza.vert sponza.frag gltf_brdf_test.comp)
    set(output "${MESH_TEST_SHADER_BINARY_DIR}/${shader}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${GLSLC_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" -o "${output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}"
            "${PROJECT_SOURCE_DIR}/src/reference/gltf_core.glsl" "${PROJECT_SOURCE_DIR}/src/reference/gltf_types.glsl"
            "${PROJECT_SOURCE_DIR}/src/reference/gltf_material.glsl" "${PROJECT_SOURCE_DIR}/src/reference/gltf_iridescence.glsl" "${PROJECT_SOURCE_DIR}/src/reference/gltf_layers.glsl" "${PROJECT_SOURCE_DIR}/src/reference/gltf_transport.glsl" VERBATIM)
    list(APPEND SPONZA_SHADER_OUTPUTS "${output}")
endforeach()
add_custom_target(vulkanizer_sponza_shaders DEPENDS ${SPONZA_SHADER_OUTPUTS})
add_dependencies(vulkanizer_sponza_test vulkanizer_sponza_shaders)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/sponza/Sponza.vkmesh")
    add_test(NAME vulkanizer_sponza_test COMMAND vulkanizer_sponza_test --timeout 3 --no-game-controller)
endif()

# Test-only wireframe and BRDF regression resources.
cmrc_add_resource_library(vulkanizer_sponza_resources NAMESPACE sponza_shaders
    WHENCE "${MESH_TEST_SHADER_BINARY_DIR}" ${SPONZA_SHADER_OUTPUTS})
add_dependencies(vulkanizer_sponza_resources vulkanizer_sponza_shaders)
target_link_libraries(vulkanizer_sponza_test PRIVATE vulkanizer_sponza_resources)

include("${PROJECT_SOURCE_DIR}/cmake/rtx_sdk.cmake")
add_executable(rtx_test rtx_test.cpp rtx_tv.cpp rtx_material.cpp rtx_neural_cache.cpp rtx_nrc.cpp rtx_nrd.cpp)
target_link_libraries(rtx_test PRIVATE vulkanizer_nrc vulkanizer_nrd)
foreach(runtime IN ITEMS NRC_Vulkan.dll cudart64_13.dll nvrtc64_130_0.dll nvrtc-builtins64_133.dll)
    add_custom_command(TARGET rtx_test POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${VULKANIZER_NRC_SDK_ROOT}/Bin/${runtime}" "$<TARGET_FILE_DIR:rtx_test>")
endforeach()
target_link_libraries(rtx_test PRIVATE vulkanizer)
target_compile_definitions(rtx_test PRIVATE
    VKZ_CAT_VIDEO="${PROJECT_SOURCE_DIR}/test/resources/video/3116737-hd_1920_1080_25fps.mp4"
    VKZ_SPONZA_MESH="${CMAKE_CURRENT_SOURCE_DIR}/resources/sponza/Sponza.vkmesh"
    VKZ_LIVING_ROOM_MESH="${CMAKE_CURRENT_SOURCE_DIR}/resources/living_room/LivingRoom.vkmesh")
set(RTX_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/rtx")
set(RTX_SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/rtx_shaders")
file(MAKE_DIRECTORY "${RTX_SHADER_BINARY_DIR}")
file(GLOB RTX_SHADER_INCLUDES CONFIGURE_DEPENDS "${RTX_SHADER_SOURCE_DIR}/*.glsl"
    "${PROJECT_SOURCE_DIR}/src/reflection/*.glsl" "${PROJECT_SOURCE_DIR}/src/reference/*.glsl")
set(RTX_SHADER_OUTPUTS)
foreach(shader IN ITEMS tv_filter.comp sponza.vert sponza.frag gbuffer.frag tone_mapping.vert tone_mapping.frag probes.vert probes.frag nrc.rgen nrc.rchit nrc.rmiss nrc.rahit nrc_resolve.comp nrd_prepare.comp nrd_resolve.comp)
    set(output "${RTX_SHADER_BINARY_DIR}/${shader}.spv")
    set(source "${RTX_SHADER_SOURCE_DIR}/${shader}")
    set(shader_options)
    if(shader STREQUAL "gbuffer.frag")
        set(source "${RTX_SHADER_SOURCE_DIR}/sponza.frag")
        list(APPEND shader_options -DGBUFFER_PASS)
    endif()
    add_custom_command(OUTPUT "${output}"
        COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.3 ${shader_options} "${source}" -o "${output}"
        DEPENDS "${source}" ${RTX_SHADER_INCLUDES} VERBATIM)
    list(APPEND RTX_SHADER_OUTPUTS "${output}")
endforeach()
foreach(entry IN ITEMS train_bridge query_bridge)
    set(output "${RTX_SHADER_BINARY_DIR}/nrc_${entry}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${VULKANIZER_DXC}" -spirv -fspv-entrypoint-name=main -T cs_6_6 -E "${entry}" -HV 2021
            -fspv-target-env=vulkan1.2 -fvk-use-dx-layout -enable-16bit-types
            "-I${VULKANIZER_NRC_SDK_ROOT}/Include" "${RTX_SHADER_SOURCE_DIR}/nrc_bridge.hlsl" -Fo "${output}"
        DEPENDS "${RTX_SHADER_SOURCE_DIR}/nrc_bridge.hlsl" "${VULKANIZER_NRC_SDK_ROOT}/Include/Nrc.hlsli"
        VERBATIM)
    list(APPEND RTX_SHADER_OUTPUTS "${output}")
endforeach()
add_custom_target(rtx_test_shaders DEPENDS ${RTX_SHADER_OUTPUTS})
cmrc_add_resource_library(rtx_test_resources NAMESPACE rtx_shaders
    WHENCE "${RTX_SHADER_BINARY_DIR}" ${RTX_SHADER_OUTPUTS})
add_dependencies(rtx_test_resources rtx_test_shaders)
target_link_libraries(rtx_test PRIVATE rtx_test_resources)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/sponza/Sponza.vkmesh")
    add_test(NAME rtx_test COMMAND rtx_test --timeout 3 --no-game-controller --validation)
    set_tests_properties(rtx_test PROPERTIES TIMEOUT 90 FAIL_REGULAR_EXPRESSION "VUID-|Validation Error")
endif()

vulkanizer_add_test_executable(vulkanizer_gltf_brdf_test gltf_brdf.cpp)
target_link_libraries(vulkanizer_gltf_brdf_test PRIVATE vulkanizer_sponza_resources)

vulkanizer_add_test_executable(vulkanizer_shader_binding_table_test shader_binding_table.cpp --verify)
set(SBT_SHADER_OUTPUTS)
foreach(shader IN ITEMS sbt.rgen sbt.rmiss sbt.rchit)
    set(output "${MESH_TEST_SHADER_BINARY_DIR}/${shader}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.3 "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" -o "${output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" VERBATIM)
    list(APPEND SBT_SHADER_OUTPUTS "${output}")
endforeach()
add_custom_target(vulkanizer_sbt_shaders DEPENDS ${SBT_SHADER_OUTPUTS})
cmrc_add_resource_library(vulkanizer_sbt_resources NAMESPACE sbt_shaders
    WHENCE "${MESH_TEST_SHADER_BINARY_DIR}" ${SBT_SHADER_OUTPUTS})
add_dependencies(vulkanizer_sbt_resources vulkanizer_sbt_shaders)
target_link_libraries(vulkanizer_shader_binding_table_test PRIVATE vulkanizer_sbt_resources)
target_compile_definitions(vulkanizer_shader_binding_table_test PRIVATE
    VKZ_BUNNY_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/resources/bunny/bunny.glb"
    VKZ_BUNNY_CONVERTED="${CMAKE_CURRENT_BINARY_DIR}/resources/bunny/bunny.vkmesh")
set_tests_properties(vulkanizer_shader_binding_table_test PROPERTIES TIMEOUT 90)

vulkanizer_add_test_executable(vulkanizer_reference_ray_tracing_test reference_ray_tracing.cpp)
set_tests_properties(vulkanizer_reference_ray_tracing_test PROPERTIES TIMEOUT 90)

vulkanizer_add_test_executable(vulkanizer_ddgi_test ddgi.cpp)
set_tests_properties(vulkanizer_ddgi_test PROPERTIES TIMEOUT 90 FAIL_REGULAR_EXPRESSION "VUID-|Validation Error")

add_executable(vulkanizer_ior_test_grid_test ior_test_grid.cpp)
target_link_libraries(vulkanizer_ior_test_grid_test PRIVATE vulkanizer vulkanizer_sponza_resources)
target_compile_definitions(vulkanizer_ior_test_grid_test PRIVATE
    VKZ_IOR_TEST_GRID_MESH="${CMAKE_CURRENT_SOURCE_DIR}/resources/ior_test_grid/IORTestGrid.vkmesh"
    VKZ_IOR_ENVIRONMENT="${CMAKE_CURRENT_SOURCE_DIR}/resources/ior_test_grid/studio_small_09_2k.hdr")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/ior_test_grid/IORTestGrid.vkmesh")
    add_test(NAME vulkanizer_ior_test_grid_graphics COMMAND vulkanizer_ior_test_grid_test --timeout 2 --validation)
    add_test(NAME vulkanizer_ior_test_grid_ray_traced COMMAND vulkanizer_ior_test_grid_test --ray-traced --timeout 2 --validation)
endif()

vulkanizer_add_test_executable(vulkanizer_ibl_test ibl.cpp)

add_executable(vulkanizer_bistro_reference_test bistro.cpp)
target_link_libraries(vulkanizer_bistro_reference_test PRIVATE vulkanizer vulkanizer_sponza_resources)
target_compile_definitions(vulkanizer_bistro_reference_test PRIVATE
    VKZ_BISTRO_MESH="${CMAKE_CURRENT_SOURCE_DIR}/resources/bistro/exterior.vkmesh"
    VKZ_BISTRO_INTERIOR_MESH="${CMAKE_CURRENT_SOURCE_DIR}/resources/bistro/interior.vkmesh"
    VKZ_BISTRO_ENVIRONMENT="${CMAKE_CURRENT_SOURCE_DIR}/resources/ior_test_grid/studio_small_09_2k.hdr")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/bistro/exterior.vkmesh")
    add_test(NAME vulkanizer_bistro_reference_graphics COMMAND vulkanizer_bistro_reference_test --timeout 3 --validation)
    add_test(NAME vulkanizer_bistro_reference_ray_traced COMMAND vulkanizer_bistro_reference_test --ray-traced --timeout 3 --validation)
    set_tests_properties(vulkanizer_bistro_reference_graphics vulkanizer_bistro_reference_ray_traced PROPERTIES TIMEOUT 180)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/bistro/interior.vkmesh")
    add_test(NAME vulkanizer_bistro_interior_graphics COMMAND vulkanizer_bistro_reference_test --interior --timeout 3 --validation)
    add_test(NAME vulkanizer_bistro_interior_ray_traced COMMAND vulkanizer_bistro_reference_test --interior --ray-traced --timeout 3 --validation)
    set_tests_properties(vulkanizer_bistro_interior_graphics vulkanizer_bistro_interior_ray_traced PROPERTIES TIMEOUT 180)
endif()

vulkanizer_add_test_executable(vulkanizer_tone_mapping_test tone_mapping.cpp)

add_executable(vulkanizer_glass_vase_flowers_test glass_vase_flowers.cpp)
target_link_libraries(vulkanizer_glass_vase_flowers_test PRIVATE vulkanizer vulkanizer_sponza_resources)
target_compile_definitions(vulkanizer_glass_vase_flowers_test PRIVATE
    VKZ_GLASS_VASE_FLOWERS_MESH="${CMAKE_CURRENT_SOURCE_DIR}/resources/glass_vase_flowers/GlassVaseFlowers.vkmesh"
    VKZ_GLASS_VASE_ENVIRONMENT="${CMAKE_CURRENT_SOURCE_DIR}/resources/ior_test_grid/studio_small_09_2k.hdr")
add_test(NAME vulkanizer_glass_vase_flowers_graphics COMMAND vulkanizer_glass_vase_flowers_test --timeout 2 --validation)
add_test(NAME vulkanizer_glass_vase_flowers_ray_traced COMMAND vulkanizer_glass_vase_flowers_test --ray-traced --timeout 2 --validation)

add_executable(vulkanizer_multi_uv_test multi_uv_test.cpp)
target_link_libraries(vulkanizer_multi_uv_test PRIVATE vulkanizer vulkanizer_sponza_resources)
target_compile_definitions(vulkanizer_multi_uv_test PRIVATE
    VKZ_MULTI_UV_MESH="${CMAKE_CURRENT_SOURCE_DIR}/resources/multi_uv_test/MultiUVTest.vkmesh"
    VKZ_MULTI_UV_ENVIRONMENT="${CMAKE_CURRENT_SOURCE_DIR}/resources/ior_test_grid/studio_small_09_2k.hdr")
add_test(NAME vulkanizer_multi_uv_graphics COMMAND vulkanizer_multi_uv_test --timeout 2 --validation)
add_test(NAME vulkanizer_multi_uv_ray_traced COMMAND vulkanizer_multi_uv_test --ray-traced --timeout 2 --validation)

add_executable(vulkanizer_material_extensions_test material_extensions.cpp)
target_link_libraries(vulkanizer_material_extensions_test PRIVATE vulkanizer vulkanizer_sponza_resources)
target_compile_definitions(vulkanizer_material_extensions_test PRIVATE
    VKZ_EXTENSION_SCENES="${CMAKE_CURRENT_SOURCE_DIR}/resources/extensions"
    VKZ_EXTENSION_ENVIRONMENT="${CMAKE_CURRENT_SOURCE_DIR}/resources/ior_test_grid/studio_small_09_2k.hdr")
foreach(scene IN ITEMS TextureTransformTest SpecularTest LightsPunctualLamp EmissiveStrengthTest UnlitTest ClearCoatTest SheenTestGrid AnisotropyStrengthTest IridescenceDielectricSpheres DispersionTest)
    add_test(NAME vulkanizer_${scene}_graphics COMMAND vulkanizer_material_extensions_test --scene ${scene} --timeout 2 --validation)
    add_test(NAME vulkanizer_${scene}_ray_traced COMMAND vulkanizer_material_extensions_test --scene ${scene} --ray-traced --timeout 2 --validation)
    set_tests_properties(vulkanizer_${scene}_graphics vulkanizer_${scene}_ray_traced PROPERTIES TIMEOUT 120)
endforeach()
