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
            // Borrowed window; owner must keep it alive while the render context is alive.
            XJGlfwWindow* mWindow = nullptr;

            // VkSurfaceKHR must be destroyed before the VkInstance it was created from.
            VkInstance mInstance = VK_NULL_HANDLE;
            VkSurfaceKHR mSurface = VK_NULL_HANDLE;
        public:
            VulkanSurface(XJGlfwWindow* window,VulkanInstance* instance);
            ~VulkanSurface();

            VulkanSurface(const VulkanSurface&) = delete;
            VulkanSurface& operator=(const VulkanSurface&) = delete;
            
            // 可选：提供获取Surface的接口
            VkSurfaceKHR XJGetSurface() { return mSurface; }
            bool IsValid() const { return mSurface != VK_NULL_HANDLE; }

            VkExtent2D XJGetFramebufferExtent() const
            {
                return mWindow ? mWindow->XJGetFramebufferExtent() : VkExtent2D{0, 0};
            }
    };
    
}


#endif