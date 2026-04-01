#include "Render/XJRenderContext.h"


namespace XJ
{
    XJRenderContext::XJRenderContext(XJGlfwWindow *mWindow)
    {
        mInstance = std::make_unique<VulkanInstance>();//创建Vulkan实例对象
        mSurface = std::make_unique<VulkanSurface>(mWindow, mInstance.get());//创建表面对象
        mPhysicalDevices = std::make_unique<VulkanPhysicalDevices>(mInstance.get(), mSurface.get());//创建物理设备对象
        mDevice = std::make_unique<XJVulkanDevice>(mPhysicalDevices.get(), 1, 1);//创建逻辑设备对象  图形队列索引  显示队列索引
        mSwapchain = std::make_unique<XJVulkanSwapchain>(mPhysicalDevices.get(), mDevice.get(), mSurface.get());//创建交换链对象

    }
    XJRenderContext::~XJRenderContext()
    {
        
    }
}