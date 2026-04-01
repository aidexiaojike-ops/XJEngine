#ifndef XJ_RENDER_CONTEXT_H
#define XJ_RENDER_CONTEXT_H


#include "Graphic/VulkanInstance.h"
#include "Graphic/VulkanSurface.h"
#include "Graphic/VulkanPhysicalDevices.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/XJVulkanSwapchain.h"

namespace XJ
{
    class XJGlfwWindow;

    class XJRenderContext
    {
        public:
            XJRenderContext(XJGlfwWindow *mWindow);
            ~XJRenderContext();

            XJVulkanSwapchain* XJGetSwapchain() const { return mSwapchain.get(); }
            XJVulkanDevice* XJGetDevice() const { return mDevice.get(); }
            VulkanSurface* XJGetSurface() const { return mSurface.get(); }
            VulkanPhysicalDevices* XJGetPhysicalDevices() const { return mPhysicalDevices.get(); }
            VulkanInstance* XJGetInstance() const { return mInstance.get(); }

        private:
            /* data */

            std::unique_ptr<VulkanInstance>    mInstance;
            std::unique_ptr<VulkanSurface>     mSurface;
            std::unique_ptr<VulkanPhysicalDevices> mPhysicalDevices;
            std::unique_ptr<XJVulkanDevice>      mDevice;
            std::unique_ptr<XJVulkanSwapchain>   mSwapchain;
    };
}


#endif