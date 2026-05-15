#添加文件
file(GLOB_RECURSE EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/UI/*.cpp
)

file(GLOB_RECURSE EDITOR_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/Public/UI/*.h
)

#add_library(Editor STATIC ${EDITOR_SOURCES} ${EDITOR_HEADERS})
add_library(Editor STATIC ${EDITOR_SOURCES})
#链接文件
target_include_directories(Editor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
        ${CMAKE_SOURCE_DIR}/Core/Public      # ← 新增：内部使用 Core 头文件
)

target_link_libraries(Editor
    PUBLIC
       Platform          # 提供 imgui、VulkanCommon、GLFW
    PRIVATE
       Core         # 提供 XJScene、XJEntity、XJComponent 等核心数据结构    
)

#target_compile_features(Editor PUBLIC cxx_std_17)
target_compile_features(Editor PRIVATE cxx_std_17)

set_target_properties(Editor PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EDITOR_SOURCES})