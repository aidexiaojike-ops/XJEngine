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

    # 清理历史遗留：旧版本可能把 DLL 拷成了名为 Vulkan 的文件。
    if(EXISTS ${VULKAN_RUNTIME_DIR} AND NOT IS_DIRECTORY ${VULKAN_RUNTIME_DIR})
        file(REMOVE ${VULKAN_RUNTIME_DIR})
    endif()
    file(MAKE_DIRECTORY ${VULKAN_RUNTIME_DIR})

    add_custom_command(
        TARGET ${VK_TARGET}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${VULKAN_RUNTIME_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${VULKAN_DLL}
            ${VULKAN_RUNTIME_DIR}
        COMMENT "Copy Vulkan runtime DLL -> bin/Vulkan"
    )
endfunction()
