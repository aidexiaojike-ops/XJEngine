#include "Graphic/VulkanSurface.h"
#include "Graphic/VulkanCommon.h"
#include <stdexcept>

namespace XJ
{
    VulkanSurface::VulkanSurface(XJGlfwWindow* window, VulkanInstance* instance)
        : mWindow(window)
    {
        if (!window || !instance)
        {
            spdlog::error("VulkanSurface::SurfaceInit 参数错误，window 或 instance 为空指针");
            throw std::runtime_error("VulkanSurface failed: window or instance is null");
        }

        mInstance = instance->XJGetInstance();
        if (mInstance == VK_NULL_HANDLE)
        {
            spdlog::error("VulkanSurface::SurfaceInit 参数错误，VkInstance 为空");
            throw std::runtime_error("VulkanSurface failed: VkInstance is null");
        }

        GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(window->XJGetImplWindowPointer());
        if (!glfwWindow)
        {
            spdlog::error("VulkanSurface::SurfaceInit 获取 GLFW window 句柄失败，XJGetImplWindowPointer 返回空指针");
            throw std::runtime_error("VulkanSurface failed: GLFW window handle is null");
        }

        VkResult result = glfwCreateWindowSurface(mInstance, glfwWindow, nullptr, &mSurface);
        XJDebug_Log(result);
        if (result != VK_SUCCESS)
        {
            spdlog::error("VulkanSurface::SurfaceInit 创建 surface 失败: {}", vk_result_string(result));
            throw std::runtime_error("VulkanSurface failed: glfwCreateWindowSurface failed");
        }

        spdlog::trace("{0} : 创建 surface 实例 : {1}", __FUNCTION__, (void*)mSurface);
    }

    VulkanSurface::~VulkanSurface()
    {
        if (mInstance != VK_NULL_HANDLE && mSurface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
            spdlog::trace("{0} : 销毁 surface 实例 : {1}", __FUNCTION__, (void*)mSurface);
            mSurface = VK_NULL_HANDLE;
        }
    }
}