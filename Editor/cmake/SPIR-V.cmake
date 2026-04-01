# ===============================
# glslc（使用项目自带 VulkanSDK）
# ===============================
set(GLSLC_COMMAND
    ${CMAKE_SOURCE_DIR}/Platform/External/VulkanSDK/Bin/glslc.exe
)

if(NOT EXISTS ${GLSLC_COMMAND})
    message(FATAL_ERROR "glslc.exe not found: ${GLSLC_COMMAND}")
endif()

# ===============================
# Shader 编译函数（工程级）
# ===============================
function(spirv_compile)
    set(options)
    set(oneValueArgs TARGET SOURCE_DIR OUTPUT_DIR)
    cmake_parse_arguments(SPIRV "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT SPIRV_TARGET)
        message(FATAL_ERROR "spirv_compile: TARGET is required")
    endif()

    file(GLOB_RECURSE SHADERS
        ${SPIRV_SOURCE_DIR}/*.vert
        ${SPIRV_SOURCE_DIR}/*.frag
        ${SPIRV_SOURCE_DIR}/*.comp
    )

    set(SPV_OUTPUTS)

    foreach(SHADER ${SHADERS})
        get_filename_component(FILE_NAME ${SHADER} NAME)
        set(SPV ${SPIRV_OUTPUT_DIR}/${FILE_NAME}.spv)

        add_custom_command(
            OUTPUT ${SPV}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_OUTPUT_DIR}
            ##COMMAND ${GLSLC_COMMAND}
            ##    ${SHADER}
            ##    -o ${SPV}
            COMMAND ${GLSLC_COMMAND}
                ${SHADER}
            #    -fshader-stage=vert  # 自动识别，无需指定
                --target-env=vulkan1.2
                -o ${SPV}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader ${FILE_NAME}"
            VERBATIM
        )

        list(APPEND SPV_OUTPUTS ${SPV})
    endforeach()

    add_custom_target(${SPIRV_TARGET} ALL
        DEPENDS PrepareRuntimeDirs ${SPV_OUTPUTS}
    )
endfunction()
