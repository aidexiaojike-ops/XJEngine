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
        public:
            VkSurfaceKHR mSurface = VK_NULL_HANDLE;

            VulkanSurface(XJGlfwWindow* window,VulkanInstance* instance);
            ~VulkanSurface();
            
            // 可选：提供获取Surface的接口
            VkSurfaceKHR XJGetSurface() { return mSurface; }
    };
    
}


#endif