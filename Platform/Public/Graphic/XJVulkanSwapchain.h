#ifndef XJ_VULAKN_SWAPCHAIN_H
#define XJ_VULAKN_SWAPCHAIN_H

#include "Edit/EditIncludes.h"
#include "Graphic/VulkanCommon.h"
//交换链
namespace XJ
{
    class XJVulkanDevice;
    class VulkanPhysicalDevices;
    class VulkanSurface;

    struct SurfaceInfo
    {
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR surfaceFormat;
        VkPresentModeKHR presentMode;
    };

    class XJVulkanSwapchain
    {
        private:
            void  SetupSurfaceCapabilities();//设置交换链
            /* data */
            VkSwapchainKHR  mSwapchain = VK_NULL_HANDLE;
            std::vector<VkImage> mImages;//存储交换链里面的图片  

            VulkanPhysicalDevices* mPhysicalDevice;
            XJVulkanDevice* mDevice;
            VulkanSurface* mSurface;

            int32_t mCurrentImageIndex = -1;//当前图片索引

            SurfaceInfo mSurfaceInfo;


        public:
            XJVulkanSwapchain(VulkanPhysicalDevices* physicalDevice, XJVulkanDevice* device, VulkanSurface* surface);
            ~XJVulkanSwapchain();

            bool ReCreate();
            
            VkResult AcquireImage(int32_t *outImageIndex,VkSemaphore semaphore, VkFence fence = VK_NULL_HANDLE);//获取图片
            VkResult Present(int32_t imageIndex, const std::vector<VkSemaphore>& waitSemaphores);
            
            VkSwapchainKHR XJGetVulkanSwapchain() { return mSwapchain; }

            const std::vector<VkImage>& XJGetSwapchainImages() const { return mImages; }
            uint32_t XJGetWidth() const { return mSurfaceInfo.capabilities.currentExtent.width; }
            uint32_t XJGetHeight() const { return mSurfaceInfo.capabilities.currentExtent.height; }

            int32_t XJGetCurrentImageIndex() const { return mCurrentImageIndex; }

            const SurfaceInfo& XJGetSurfaceInfo() const { return mSurfaceInfo; }
            
    };
    

    
}

#endif