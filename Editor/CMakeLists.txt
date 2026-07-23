#添加文件
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/UI/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Services/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Controllers/*.cpp
)

file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Public/UI/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Public/Services/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Public/Controllers/*.h

)

add_library(Editor STATIC ${EDITOR_SOURCES} ${EDITOR_HEADERS})
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
       imgui
    PRIVATE
       Core         # 提供 XJScene、XJEntity、XJComponent 等核心数据结构    
       nlohmann_json
)

#target_compile_features(Editor PUBLIC cxx_std_20)
target_compile_features(Editor PRIVATE cxx_std_20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Editor PRIVATE -Wa,-mbig-obj)
endif()

set_target_properties(Editor PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EDITOR_SOURCES} ${EDITOR_HEADERS})
