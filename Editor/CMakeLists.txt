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
)

target_link_libraries(Editor
    PUBLIC
       Platform          # 提供 imgui、VulkanCommon、GLFW
)

#target_compile_features(Editor PUBLIC cxx_std_17)
target_compile_features(Editor PRIVATE cxx_std_17)

set_target_properties(Editor PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EDITOR_SOURCES})