# Reuse the SDK installations used by the sibling light_transport reference.
# These cache paths can point at independent SDK installations/builds.
set(VULKANIZER_NRC_SDK_ROOT "${PROJECT_SOURCE_DIR}/../light_transport/_deps/nrc-sdk" CACHE PATH "NVIDIA NRC SDK root")
set(VULKANIZER_NRD_SDK_ROOT "${PROJECT_SOURCE_DIR}/../light_transport/_deps/nrd-sdk" CACHE PATH "NVIDIA NRD SDK root")
set(VULKANIZER_NRD_LIBRARY "${PROJECT_SOURCE_DIR}/../light_transport/cmake-build-debug/_deps/nrd-build/Debug/NRD.lib" CACHE FILEPATH "NRD static library with embedded SPIR-V, normal encoding 2, roughness encoding 1")

set(VULKANIZER_NRD_BLOB_LIBRARY "${PROJECT_SOURCE_DIR}/../light_transport/cmake-build-debug/_deps/shadermake-build/Debug/ShaderMakeBlob.lib" CACHE FILEPATH "NRD ShaderMake blob library")

foreach(required IN ITEMS "${VULKANIZER_NRC_SDK_ROOT}/Include/NrcVk.h" "${VULKANIZER_NRC_SDK_ROOT}/Lib/NRC_Vulkan.lib"
        "${VULKANIZER_NRD_SDK_ROOT}/Include/NRD.h" "${VULKANIZER_NRD_LIBRARY}" "${VULKANIZER_NRD_BLOB_LIBRARY}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "rtx_test SDK dependency missing: ${required}. Set VULKANIZER_NRC_SDK_ROOT, VULKANIZER_NRD_SDK_ROOT and VULKANIZER_NRD_LIBRARY.")
    endif()
endforeach()

add_library(vulkanizer_nrc SHARED IMPORTED)
set_target_properties(vulkanizer_nrc PROPERTIES
    IMPORTED_IMPLIB "${VULKANIZER_NRC_SDK_ROOT}/Lib/NRC_Vulkan.lib"
    IMPORTED_LOCATION "${VULKANIZER_NRC_SDK_ROOT}/Bin/NRC_Vulkan.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${VULKANIZER_NRC_SDK_ROOT}/Include")
add_library(vulkanizer_nrd STATIC IMPORTED)
set_target_properties(vulkanizer_nrd PROPERTIES
    IMPORTED_LOCATION "${VULKANIZER_NRD_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${VULKANIZER_NRD_SDK_ROOT}/Include"
    INTERFACE_LINK_LIBRARIES "${VULKANIZER_NRD_BLOB_LIBRARY}")
find_program(VULKANIZER_DXC dxc HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
