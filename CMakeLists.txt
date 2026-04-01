cmake_minimum_required(VERSION 3.10)

# ------------------------------------------------------------
# 项目声明
# ------------------------------------------------------------
project(XJEngine LANGUAGES CXX)

# ------------------------------------------------------------
# 基础编译设置
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------
# 【★ 核心】统一运行期输出目录（引擎级约定）
# 所有配置（Debug / Release）都输出到 bin/
# ------------------------------------------------------------
set(XJ_RUNTIME_DIR        ${CMAKE_SOURCE_DIR}/bin)
set(XJ_RESOURCE_DIR       ${XJ_RUNTIME_DIR}/Resource)
set(XJ_SHADER_OUTPUT_DIR  ${XJ_RESOURCE_DIR}/Shader)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${XJ_RUNTIME_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${XJ_RUNTIME_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${XJ_RUNTIME_DIR})

# ------------------------------------------------------------
# 【★ 运行期目录准备】
# 这是“资源阶段”的根，不只是 Shader
# ------------------------------------------------------------
add_custom_target(PrepareRuntimeDirs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XJ_RUNTIME_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XJ_RESOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XJ_SHADER_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XJ_RUNTIME_DIR}/log
)

# ------------------------------------------------------------
# CMake 模块路径
# ------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/Editor/cmake
)

# ------------------------------------------------------------
# 引入 CMake 功能模块
# ------------------------------------------------------------
include(SPIR-V)          # Shader → SPIR-V 编译
include(FindVulkanDLLs)  # Vulkan Runtime DLL 查找 / 拷贝

# ------------------------------------------------------------
# 外部 / 平台模块
# ------------------------------------------------------------
add_subdirectory(Platform/External/VulkanSDK)
add_subdirectory(Platform)

# ------------------------------------------------------------
# Core 模块（可选，如果存在）
# ------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/Core/CMakeLists.txt)
    add_subdirectory(Core)
    message(STATUS "Core module: ON")
else()
    message(STATUS "Core module: OFF (Core/CMakeLists.txt not found)")
endif()

# ------------------------------------------------------------
# 主程序源文件
# ------------------------------------------------------------
file(GLOB ENGINE_SRC
    ${CMAKE_SOURCE_DIR}/Src/*.cpp
)

add_executable(XJEngine ${ENGINE_SRC})#会打开cmd
#add_executable(XJEngine WIN32 ${ENGINE_SRC})

target_link_libraries(XJEngine
    PRIVATE Platform
    PRIVATE Core # 如果 Core 模块存在，则链接它
)

# ------------------------------------------------------------
# 【★ Shader 编译（不进 exe）】
# 构建期资源 → bin/Resource/Shader
# ------------------------------------------------------------
spirv_compile(
    TARGET CompileShaders
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/Resource/Shader
    OUTPUT_DIR ${XJ_SHADER_OUTPUT_DIR}
)

# Shader 必须先于 exe
add_dependencies(CompileShaders PrepareRuntimeDirs)
add_dependencies(XJEngine CompileShaders)

# ------------------------------------------------------------
# 【★ Vulkan Runtime DLL】
# 保证 exe 同目录可运行
# ------------------------------------------------------------
copy_vulkan_runtime_dlls(
    TARGET XJEngine
    OUTPUT_DIR ${XJ_RUNTIME_DIR}
)

# ------------------------------------------------------------
# 【★ 非 Shader 资源复制】
# 注意：Shader 已由 glslc 生成，这里不应覆盖
# ------------------------------------------------------------
add_custom_command(TARGET XJEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Resource/Texture
        ${XJ_RESOURCE_DIR}/Texture
)

add_custom_command(TARGET XJEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Resource/Mesh
        ${XJ_RESOURCE_DIR}/Mesh
)

add_custom_command(TARGET XJEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Resource/Config
        ${XJ_RESOURCE_DIR}/Config
)

# ------------------------------------------------------------
# 工程状态提示（可选，但对调试很有用）
# ------------------------------------------------------------
message(STATUS "XJEngine runtime dir  : ${XJ_RUNTIME_DIR}")
message(STATUS "XJEngine shader output: ${XJ_SHADER_OUTPUT_DIR}")