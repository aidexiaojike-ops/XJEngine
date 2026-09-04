cmake_minimum_required(VERSION 3.10)

# ------------------------------------------------------------
# 项目声明
# ------------------------------------------------------------
project(XJEngine LANGUAGES CXX)

# ------------------------------------------------------------
# 基础编译设置
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------
# 【★ 核心】统一运行期输出目录（引擎级约定）
# 所有配置（Debug / Release）都输出到 bin/
#
# 开发阶段：源 Resource 是唯一资产真相，编辑器与运行时都直接读取
# ${CMAKE_SOURCE_DIR}/Resource，不再维护 bin/Resource 副本。
# 最终 Export/Package 阶段才把资产整理进游戏目录（后续实现）。
# ------------------------------------------------------------
set(XJ_RUNTIME_DIR        ${CMAKE_SOURCE_DIR}/bin)
set(XJ_SHADER_OUTPUT_DIR  ${CMAKE_SOURCE_DIR}/Resource/Shader)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${XJ_RUNTIME_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${XJ_RUNTIME_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${XJ_RUNTIME_DIR})

# ------------------------------------------------------------
# 【★ 运行期目录准备】
# exe 与日志输出目录。
# ------------------------------------------------------------
add_custom_target(PrepareRuntimeDirs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XJ_RUNTIME_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XJ_RUNTIME_DIR}/log
)

# ------------------------------------------------------------
# CMake 模块路径
# ------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/cmake
)

# ------------------------------------------------------------
# 引入 CMake 功能模块
# ------------------------------------------------------------
include(SetupVulkanSDK)  # Resolve Vulkan SDK from env/local cache/download
include(SPIR-V)          # Shader → SPIR-V 编译
include(FindVulkanDLLs)  # Vulkan Runtime DLL 查找 / 拷贝

# ------------------------------------------------------------
# 外部 / 平台模块
# ------------------------------------------------------------
include(${CMAKE_SOURCE_DIR}/cmake/ThirdParty.cmake)
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
# Editor  模块（可选，如果存在）
# ------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/Editor/CMakeLists.txt)
    add_subdirectory(Editor)
    message(STATUS "Editor module: ON")
else()
    message(STATUS "Editor module: OFF (Editor/CMakeLists.txt not found)")
endif()
# ------------------------------------------------------------
# UTF-8
# ------------------------------------------------------------
# 为 MSVC 编译器添加 UTF-8 支持
if(MSVC)
    add_compile_options(/utf-8)
    message(STATUS "MSVC: Enabled UTF-8 support (/utf-8)")
endif()

# 为 Clang/GCC/MinGW 编译器添加 UTF-8 支持
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GCC")
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    message(STATUS "GCC/Clang/MinGW: Enabled UTF-8 support (-finput-charset=UTF-8 -fexec-charset=UTF-8)")
endif()
# ------------------------------------------------------------
# 主程序源文件
# ------------------------------------------------------------
file(GLOB ENGINE_SRC
    ${CMAKE_SOURCE_DIR}/Src/*.cpp
)
#代码量过大  添加编译
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   add_compile_options(-Wa,-mbig-obj)
endif()

add_executable(XJEngine ${ENGINE_SRC})#会打开cmd
#add_executable(XJEngine WIN32 ${ENGINE_SRC})

# 编辑器开发版本的默认项目根目录。
# 后续命令行 --project 会覆盖这个值。
file(TO_CMAKE_PATH
    "${CMAKE_SOURCE_DIR}"
    XJ_DEFAULT_PROJECT_ROOT_PATH
)

target_compile_definitions(XJEngine
    PRIVATE
        XJ_DEFAULT_PROJECT_ROOT="${XJ_DEFAULT_PROJECT_ROOT_PATH}"
)

target_link_libraries(XJEngine
    PRIVATE Platform
    PRIVATE Core # 如果 Core 模块存在，则链接它
    PRIVATE Editor
)

# ------------------------------------------------------------
# 【★ Shader 编译（不进 exe）】
# 构建期资源 → 源项目 Resource/Shader（.spv 与 .xjshader 同源）
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
# 开发阶段不复制源资产到 bin：编辑器与运行时都直读源 Resource。
# 最终 Export/Package 步骤（后续实现）才把资产整理进游戏目录。
# ------------------------------------------------------------
message(STATUS "XJEngine runtime dir  : ${XJ_RUNTIME_DIR}")
message(STATUS "XJEngine shader output: ${XJ_SHADER_OUTPUT_DIR}")
