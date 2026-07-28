#ifndef VULKAN_SURFACE_H
#define VULKAN_SURFACE_H


#include "Edit/EditIncludes.h"
#include "Edit/XJGlfwWindow.h"
#include "Graphic/VulkanInstance.h"

namespace XJ
{
    class VulkanSurface
    {
        private:
            /* data */
            VulkanInstance* mInstance; // 保存指针
            XJGlfwWindow* mWindow = nullptr;
        public:
            VkSurfaceKHR mSurface = VK_NULL_HANDLE;

            VulkanSurface(XJGlfwWindow* window,VulkanInstance* instance);
            ~VulkanSurface();
            VulkanSurface(const VulkanSurface&) = delete;
            VulkanSurface& operator=(const VulkanSurface&) = delete;
            
            // 可选：提供获取Surface的接口
            VkSurfaceKHR XJGetSurface() { return mSurface; }

            VkExtent2D XJGetFramebufferExtent() const
            {
                return mWindow ? mWindow->XJGetFramebufferExtent() : VkExtent2D{0, 0};
            }
    };
    
}


#endif