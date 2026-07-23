# 引入模块
#file(GLOB PLATFORM_SRC
#    ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp
#)

# 替换原来的 file(GLOB PLATFORM_SRC)
set(PLATFORM_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Edit/SpdlogDebug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Edit/XJGlfwWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Edit/XJEventTesting.cpp

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


# 设置 Platform 的 include 路径
target_include_directories(Platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
        #${CMAKE_CURRENT_SOURCE_DIR}/External/spdlog  # spdlog 的根目录（含 spdlog/ 子目录）
)

# 设置 C++ 标准
target_compile_features(Platform PUBLIC cxx_std_20)

# 禁用 spdlog/fmt 的弃用警告
if(MSVC)
    target_compile_options(Platform PRIVATE /wd4996)
else()
    target_compile_options(Platform PRIVATE -Wno-deprecated-declarations)
endif()

# ----------- 链接第三方库 -----------
# 所有依赖的目标都已经被定义，可以安全链接
target_link_libraries(Platform 
    PUBLIC
        Vulkan::Vulkan
        glfw
        imgui
        glm::glm-header-only
        spdlog
        stb
)
