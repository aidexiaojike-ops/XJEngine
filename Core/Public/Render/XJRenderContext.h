#ifndef XJ_RENDER_CONTEXT_H
#define XJ_RENDER_CONTEXT_H


#include "Graphic/XJVulkanInstance.h"
#include "Graphic/XJVulkanSurface.h"
#include "Graphic/XJVulkanPhysicalDevices.h"
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
            XJVulkanSurface* XJGetSurface() const { return mSurface.get(); }
            XJVulkanPhysicalDevices* XJGetPhysicalDevices() const { return mPhysicalDevices.get(); }
            XJVulkanInstance* XJGetInstance() const { return mInstance.get(); }

        private:
            /* data */

            std::unique_ptr<XJVulkanInstance>    mInstance;
            std::unique_ptr<XJVulkanSurface>     mSurface;
            std::unique_ptr<XJVulkanPhysicalDevices> mPhysicalDevices;
            std::unique_ptr<XJVulkanDevice>      mDevice;
            std::unique_ptr<XJVulkanSwapchain>   mSwapchain;
    };
}


#endif