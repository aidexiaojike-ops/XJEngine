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
    # Vulkan Runtime DLL (from bundled VulkanSDK)
    # ---------------------------------------------
    set(VULKAN_DLL
        ${CMAKE_SOURCE_DIR}/Platform/External/VulkanSDK/Dll/vulkan-1.dll
    )

    if(NOT EXISTS ${VULKAN_DLL})
        message(FATAL_ERROR
            "Vulkan runtime DLL not found:\n  ${VULKAN_DLL}\n"
            "Please check Platform/External/VulkanSDK/Dll/"
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
