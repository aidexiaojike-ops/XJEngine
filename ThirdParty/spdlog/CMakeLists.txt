# 设置最小 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 项目名称（可以随意取，不影响主项目）
project(spdlog LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 定义一个接口库（INTERFACE 表示只包含头文件，没有源文件需要编译）
add_library(spdlog INTERFACE)

# 指定 spdlog 的头文件目录
# INTERFACE_INCLUDE_DIRECTORIES 会让使用 spdlog 的目标自动包含这些目录
target_include_directories(spdlog INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>  # 构建时的路径
)

# 如果 spdlog 需要链接其他库（比如线程库），也在这里添加
# spdlog 默认需要线程支持，所以链接 Threads::Threads
find_package(Threads REQUIRED)
target_link_libraries(spdlog INTERFACE Threads::Threads)
