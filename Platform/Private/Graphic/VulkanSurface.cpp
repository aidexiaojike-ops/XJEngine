#include "Graphic/VulkanSurface.h"
#include "Graphic/VulkanCommon.h"

namespace XJ
{
    
    VulkanSurface::VulkanSurface(XJGlfwWindow* window,VulkanInstance* instance) : mInstance(instance)
    {
        if(!window || !instance)
        {
            spdlog::error("VulkanSurface::SurfaceInit 参数错误，window 或 instance 为空指针");
            return;
        }
        auto *glfWwindow = dynamic_cast<XJGlfwWindow*>(window);
        if(!glfWwindow)
        {
            //Faild to get GLFW window handle
            spdlog::error("VulkanSurface::SurfaceInit 参数错误：传入的window不是XJGlfwWindow类型");
            return;
        }
      
        GLFWwindow* XJGetImplWindowPointer = static_cast<GLFWwindow*>(glfWwindow->XJGetImplWindowPointer());
        if(XJGetImplWindowPointer == nullptr)
        {   
            spdlog::error("VulkanSurface::SurfaceInit 获取 GLFW window 句柄失败，XJGetImplWindowPointer 返回空指针");
            return;
        }
        XJDebug_Log(glfwCreateWindowSurface(mInstance->XJGetInstance(), XJGetImplWindowPointer, nullptr, &mSurface));
        spdlog::trace("{0} : 创建 surface 实例 : {1}", __FUNCTION__, (void*)mSurface);
        //glfwCreateWindowSurface(instance->XJGetVulkanInstance(), window->XJGetWindow(), nullptr, &surface);
        //待实现

    }
    VulkanSurface::~VulkanSurface()
    {
        // 销毁 surface，例如：
        if (mSurface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(mInstance->XJGetInstance(), mSurface, nullptr);
            mSurface = VK_NULL_HANDLE;
        }
        spdlog::trace("{0} : 销毁 surface 实例 : {1}", __FUNCTION__, (void*)mInstance);
        //待实现
    }
}