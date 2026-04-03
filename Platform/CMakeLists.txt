# 引入模块
#file(GLOB PLATFORM_SRC
#    ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp
#)

# 替换原来的 file(GLOB PLATFORM_SRC)
set(PLATFORM_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Edit/SpdlogDebug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Edit/XJGlfwWindow.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/VulkanInstance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/VulkanSurface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/VulkanPhysicalDevices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanDevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/VulkanQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanSwapchain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanRenderPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanFrameBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/VulkanImageView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanPipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanCommandBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanAddModelAssets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanImage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanGeometryUtil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanDepthImage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanDescriptorSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Graphic/XJVulkanTextureSampler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Event/XJEventDispatcher.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Private/XJEngine.cpp
    # 后续新增文件直接添加在这里
)
add_library(Platform STATIC ${PLATFORM_SRC})
# 找到 add_library(Platform ...) 这一行，在后面添加：
#target_compile_definitions(Platform PUBLIC
#    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE  # 启用 trace 级别（包含所有更低级别）
#)


# 检查Core是否存在并链接
if(TARGET Core)
    message(STATUS "Linking Core to Platform")
    target_link_libraries(Platform
        PUBLIC
            Core
    )
endif()

# 设置 Platform 的 include 路径
target_include_directories(Platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
        #${CMAKE_CURRENT_SOURCE_DIR}/External/spdlog  # spdlog 的根目录（含 spdlog/ 子目录）
)

# 设置 C++ 标准
target_compile_features(Platform PUBLIC cxx_std_17)

# ----------- 添加第三方库 (顺序很重要) -----------

# 1. Vulkan
# 链接 Vulkan（来自 External，而不是系统）

target_link_libraries(Platform
    PUBLIC
        Vulkan::Vulkan
)

# 2. GLFW
set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/glfw)
add_subdirectory(${GLFW_DIR})

# 3. ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/imgui)
add_subdirectory(${IMGUI_DIR})

# 4.GLM
set(GLM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/glm)
add_subdirectory(${GLM_DIR})

# 5. tinyobjloader
set(TINYOBJLOADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/tinyobjloader)
add_subdirectory(${TINYOBJLOADER_DIR})

# 4. spdlog
#    这一步会执行 spdlog 的 CMakeLists.txt，从而创建 spdlog::spdlog 目标
set(SPDLOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/spdlog)
add_subdirectory(${SPDLOG_DIR})
#5.stb
set(STB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/stb)
add_subdirectory(${STB_DIR})
#6.entt
set(ENTT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/entt)
add_subdirectory(${ENTT_DIR})

# ----------- 链接第三方库 -----------
# 所有依赖的目标都已经被定义，可以安全链接
target_link_libraries(Platform 
    PUBLIC
        Vulkan::Vulkan
        glfw
        imgui
        spdlog # 现在 CMake 认识这个目标了
        glm::glm-header-only
        tinyobjloader
        stb
        EnTT::EnTT
)

set(SHADER_ROOT ${CMAKE_SOURCE_DIR}/Resource/Shader)
#shader Files
set(SHADERS
    ${SHADER_ROOT}/BaseVertex.vert
    ${SHADER_ROOT}/BaseVertex.frag
    ${SHADER_ROOT}/Descriptor.vert
    ${SHADER_ROOT}/Descriptor.frag
)

