# ...existing code...
cmake_minimum_required(VERSION 3.15)
project(imgui)

# 收集ImGui源文件
file(GLOB IMGUI_SOURCES
    "*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_tables.cpp"
)

# 收集后端源文件，默认包含 GLFW 后端
set(IMGUI_BACKENDS
    "backends/imgui_impl_glfw.cpp"
)

# 根据上层 HAVE_VULKAN 决定是否加入 Vulkan 后端
if(DEFINED HAVE_VULKAN AND HAVE_VULKAN)
    list(APPEND IMGUI_BACKENDS "backends/imgui_impl_vulkan.cpp")
    set(IMGUI_USE_VULKAN TRUE)
else()
    set(IMGUI_USE_VULKAN FALSE)
endif()

# 创建库
add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_BACKENDS})

# 设置包含目录
target_include_directories(imgui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

# 链接必要的依赖（有 Vulkan 时链接 Vulkan）
if(IMGUI_USE_VULKAN)
    target_link_libraries(imgui
        PUBLIC
            glfw
            Vulkan::Vulkan
    )
else()
    target_link_libraries(imgui
        PUBLIC
            glfw
    )
endif()