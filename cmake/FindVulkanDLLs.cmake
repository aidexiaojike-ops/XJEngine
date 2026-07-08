# =========================================================
# Copy Vulkan Runtime DLLs (Engine-level, Windows only)
# =========================================================
function(copy_vulkan_runtime_dlls)
    set(options)
    set(oneValueArgs TARGET OUTPUT_DIR)
    cmake_parse_arguments(VK "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT WIN32)
        return()
    endif()

    # ---------------------------------------------
    # Vulkan Runtime DLL resolved by SetupVulkanSDK.cmake
    # ---------------------------------------------
    if(NOT DEFINED XJ_VULKAN_DLL)
        message(FATAL_ERROR "XJ_VULKAN_DLL is not defined. Include SetupVulkanSDK before FindVulkanDLLs.cmake")
    endif()

    set(VULKAN_DLL ${XJ_VULKAN_DLL})

    if(NOT EXISTS ${VULKAN_DLL})
        message(FATAL_ERROR
            "Vulkan runtime DLL not found:\n  ${VULKAN_DLL}\n"
            "Please check the resolved Vulkan SDK path."
        )
    endif()

    # ---------------------------------------------
    # Runtime destination
    # bin/Vulkan/
    # ---------------------------------------------
    set(VULKAN_RUNTIME_DIR ${VK_OUTPUT_DIR}/Vulkan)
    file(MAKE_DIRECTORY ${VULKAN_RUNTIME_DIR})

    add_custom_command(
        TARGET ${VK_TARGET}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${VULKAN_DLL}
            ${VULKAN_RUNTIME_DIR}
        COMMENT "Copy Vulkan runtime DLL -> bin/Vulkan"
    )
endfunction()
