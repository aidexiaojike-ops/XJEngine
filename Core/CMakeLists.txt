# ============================================================
# Core 模块 - 游戏引擎核心层
# ============================================================

# 检查是否有源文件
file(GLOB_RECURSE CORE_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/Public/*.h
)

file(GLOB_RECURSE CORE_PRIVATE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.h
)

file(GLOB_RECURSE CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp
)

# 如果有源文件，则创建静态库
if(CORE_SOURCES)
    add_library(Core STATIC 
        ${CORE_SOURCES}
        ${CORE_PUBLIC_HEADERS}
        ${CORE_PRIVATE_HEADERS}
    )
    
    # 设置Core的include路径
    target_include_directories(Core
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/Public
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Private
    )
    
    # 链接Platform（Core依赖Platform）
    target_link_libraries(Core
        PUBLIC
            Platform
    )
    
    # 设置C++标准
    target_compile_features(Core PUBLIC cxx_std_17)
    
else()
    # 如果没有源文件，创建接口库（仅头文件）
    add_library(Core INTERFACE)
    
    target_include_directories(Core
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/Public
            ${CMAKE_CURRENT_SOURCE_DIR}/Private
    )
endif()

# 设置输出目录
set_target_properties(Core PROPERTIES
    OUTPUT_NAME "Core"
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)