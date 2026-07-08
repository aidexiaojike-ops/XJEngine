# ============================================================
# Vulkan SDK setup for XJEngine
#
# Resolution order:
#   1. Use the user's installed SDK from the VULKAN_SDK env var.
#   2. Use the local cached SDK at ThirdParty/VulkanSDK.
#   3. Download LunarG Vulkan SDK 1.3.283.0 and install/extract it locally.
#
# This module exports:
#   XJ_VULKAN_SDK_PATH
#   XJ_VULKAN_INCLUDE_DIR
#   XJ_VULKAN_LIBRARY
#   XJ_VULKAN_GLSLC
#   XJ_VULKAN_DLL
#   Vulkan::Vulkan
# ============================================================

set(XJ_VULKAN_SDK_VERSION "1.3.283.0" CACHE STRING "Vulkan SDK version used by XJEngine")
set(XJ_VULKAN_SDK_LOCAL_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/VulkanSDK" CACHE PATH "Local Vulkan SDK cache directory")

# LunarG publishes Windows SDK installers at stable versioned URLs. The URL is
# still configurable so future SDK upgrades only need a cache/CMake update.
set(XJ_VULKAN_SDK_URL
    "https://sdk.lunarg.com/sdk/download/1.3.283.0/windows/VulkanSDK-1.3.283.0-Installer.exe"
    CACHE STRING
    "Vulkan SDK installer/archive URL used when no SDK is installed or cached"
)
set(XJ_VULKAN_DOWNLOAD_TIMEOUT 1800 CACHE STRING "Vulkan SDK download timeout in seconds")

function(xj_detect_vulkan_sdk SDK_ROOT OUT_VALID)
    if(NOT SDK_ROOT)
        set(${OUT_VALID} FALSE PARENT_SCOPE)
        return()
    endif()

    if(EXISTS "${SDK_ROOT}/Include/vulkan/vulkan.h" AND
       EXISTS "${SDK_ROOT}/Lib/vulkan-1.lib" AND
       EXISTS "${SDK_ROOT}/Bin/glslc.exe")
        set(${OUT_VALID} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VALID} FALSE PARENT_SCOPE)
    endif()
endfunction()

set(_xj_vulkan_resolved "")
set(_xj_vulkan_source "")

if(DEFINED ENV{VULKAN_SDK})
    file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" _xj_env_vulkan_sdk)
    xj_detect_vulkan_sdk("${_xj_env_vulkan_sdk}" _xj_env_vulkan_valid)
    if(_xj_env_vulkan_valid)
        set(_xj_vulkan_resolved "${_xj_env_vulkan_sdk}")
        set(_xj_vulkan_source "environment variable VULKAN_SDK")
    endif()
endif()

if(NOT _xj_vulkan_resolved)
    xj_detect_vulkan_sdk("${XJ_VULKAN_SDK_LOCAL_DIR}" _xj_local_vulkan_valid)
    if(_xj_local_vulkan_valid)
        set(_xj_vulkan_resolved "${XJ_VULKAN_SDK_LOCAL_DIR}")
        set(_xj_vulkan_source "local ThirdParty/VulkanSDK cache")
    endif()
endif()

if(NOT _xj_vulkan_resolved)
    if(NOT XJ_VULKAN_SDK_URL)
        message(FATAL_ERROR
            "Vulkan SDK ${XJ_VULKAN_SDK_VERSION} was not found.\n"
            "Checked:\n"
            "  1) VULKAN_SDK environment variable\n"
            "  2) ${XJ_VULKAN_SDK_LOCAL_DIR}\n\n"
            "Install LunarG Vulkan SDK ${XJ_VULKAN_SDK_VERSION}, place it at ThirdParty/VulkanSDK, "
            "or configure XJ_VULKAN_SDK_URL."
        )
    endif()

    get_filename_component(_xj_vulkan_url_name "${XJ_VULKAN_SDK_URL}" NAME)
    if(NOT _xj_vulkan_url_name)
        set(_xj_vulkan_url_name "VulkanSDK-${XJ_VULKAN_SDK_VERSION}")
    endif()

    set(_xj_vulkan_download "${CMAKE_BINARY_DIR}/_deps/${_xj_vulkan_url_name}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/ThirdParty")

    message(STATUS "Downloading Vulkan SDK ${XJ_VULKAN_SDK_VERSION} from ${XJ_VULKAN_SDK_URL}")
    message(STATUS "This is a large download and may take several minutes on first configure.")
    file(DOWNLOAD
        "${XJ_VULKAN_SDK_URL}"
        "${_xj_vulkan_download}"
        SHOW_PROGRESS
        TIMEOUT ${XJ_VULKAN_DOWNLOAD_TIMEOUT}
        TLS_VERIFY ON
        STATUS _xj_vulkan_download_status
    )

    list(GET _xj_vulkan_download_status 0 _xj_vulkan_download_code)
    if(NOT _xj_vulkan_download_code EQUAL 0)
        message(FATAL_ERROR
            "Failed to download Vulkan SDK: ${_xj_vulkan_download_status}\n"
            "If the network is slow, retry or increase -DXJ_VULKAN_DOWNLOAD_TIMEOUT=<seconds>.\n"
            "You can also install LunarG Vulkan SDK ${XJ_VULKAN_SDK_VERSION} manually or place it at ThirdParty/VulkanSDK."
        )
    endif()

    file(REMOVE_RECURSE "${XJ_VULKAN_SDK_LOCAL_DIR}")

    get_filename_component(_xj_vulkan_download_ext "${_xj_vulkan_download}" EXT)
    string(TOLOWER "${_xj_vulkan_download_ext}" _xj_vulkan_download_ext)

    if(_xj_vulkan_download_ext STREQUAL ".exe")
        if(NOT WIN32)
            message(FATAL_ERROR "The default Vulkan SDK download is a Windows installer and can only run on Windows.")
        endif()

        file(MAKE_DIRECTORY "${XJ_VULKAN_SDK_LOCAL_DIR}")
        message(STATUS "Installing Vulkan SDK to ${XJ_VULKAN_SDK_LOCAL_DIR}")
        execute_process(
            COMMAND "${_xj_vulkan_download}"
                --root "${XJ_VULKAN_SDK_LOCAL_DIR}"
                --accept-licenses
                --default-answer
                --confirm-command
                install
            RESULT_VARIABLE _xj_vulkan_install_result
        )

        if(NOT _xj_vulkan_install_result EQUAL 0)
            message(FATAL_ERROR "Failed to install Vulkan SDK installer: ${_xj_vulkan_download}")
        endif()
    else()
        file(MAKE_DIRECTORY "${XJ_VULKAN_SDK_LOCAL_DIR}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf "${_xj_vulkan_download}"
            WORKING_DIRECTORY "${XJ_VULKAN_SDK_LOCAL_DIR}"
            RESULT_VARIABLE _xj_vulkan_extract_result
        )

        if(NOT _xj_vulkan_extract_result EQUAL 0)
            message(FATAL_ERROR "Failed to extract Vulkan SDK archive: ${_xj_vulkan_download}")
        endif()
    endif()

    # Accept either an archive that extracts directly into SDK files, or one
    # that extracts into a single versioned child directory.
    xj_detect_vulkan_sdk("${XJ_VULKAN_SDK_LOCAL_DIR}" _xj_download_direct_valid)
    if(_xj_download_direct_valid)
        set(_xj_vulkan_resolved "${XJ_VULKAN_SDK_LOCAL_DIR}")
    else()
        file(GLOB _xj_vulkan_children LIST_DIRECTORIES TRUE "${XJ_VULKAN_SDK_LOCAL_DIR}/*")
        foreach(_xj_child IN LISTS _xj_vulkan_children)
            if(IS_DIRECTORY "${_xj_child}")
                xj_detect_vulkan_sdk("${_xj_child}" _xj_child_valid)
                if(_xj_child_valid)
                    set(_xj_vulkan_resolved "${_xj_child}")
                    break()
                endif()
            endif()
        endforeach()
    endif()

    if(NOT _xj_vulkan_resolved)
        message(FATAL_ERROR
            "Downloaded Vulkan SDK archive did not contain the expected Windows SDK layout.\n"
            "Expected Include/vulkan/vulkan.h, Lib/vulkan-1.lib, and Bin/glslc.exe."
        )
    endif()

    set(_xj_vulkan_source "downloaded archive")
endif()

set(XJ_VULKAN_SDK_PATH "${_xj_vulkan_resolved}" CACHE PATH "Resolved Vulkan SDK root" FORCE)
set(XJ_VULKAN_INCLUDE_DIR "${XJ_VULKAN_SDK_PATH}/Include" CACHE PATH "Vulkan include directory" FORCE)
set(XJ_VULKAN_LIBRARY "${XJ_VULKAN_SDK_PATH}/Lib/vulkan-1.lib" CACHE FILEPATH "Vulkan import library" FORCE)
set(XJ_VULKAN_GLSLC "${XJ_VULKAN_SDK_PATH}/Bin/glslc.exe" CACHE FILEPATH "Vulkan shader compiler" FORCE)
set(XJ_VULKAN_DLL "${XJ_VULKAN_SDK_PATH}/Dll/vulkan-1.dll" CACHE FILEPATH "Vulkan runtime DLL" FORCE)

foreach(_xj_required_file IN ITEMS
    "${XJ_VULKAN_INCLUDE_DIR}/vulkan/vulkan.h"
    "${XJ_VULKAN_LIBRARY}"
    "${XJ_VULKAN_GLSLC}"
    "${XJ_VULKAN_DLL}"
)
    if(NOT EXISTS "${_xj_required_file}")
        message(FATAL_ERROR "Required Vulkan SDK file not found: ${_xj_required_file}")
    endif()
endforeach()

if(NOT TARGET Vulkan::Vulkan)
    add_library(Vulkan INTERFACE)
    target_include_directories(Vulkan INTERFACE "${XJ_VULKAN_INCLUDE_DIR}")
    target_link_libraries(Vulkan INTERFACE "${XJ_VULKAN_LIBRARY}")
    add_library(Vulkan::Vulkan ALIAS Vulkan)
endif()

message(STATUS "Vulkan SDK source : ${_xj_vulkan_source}")
message(STATUS "Vulkan SDK path   : ${XJ_VULKAN_SDK_PATH}")
message(STATUS "Vulkan glslc      : ${XJ_VULKAN_GLSLC}")
