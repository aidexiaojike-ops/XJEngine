#ifndef XJ_VULKAN_SWAPCHAIN_H
#define XJ_VULKAN_SWAPCHAIN_H

#include "Edit/EditIncludes.h"
#include "Graphic/VulkanCommon.h"
//交换链
namespace XJ
{
    class XJVulkanDevice;
    class XJVulkanPhysicalDevices;
    class XJVulkanSurface;

    struct SurfaceInfo
    {
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR surfaceFormat;
        VkPresentModeKHR presentMode;
    };

    struct XJSwapchainAcquireResult
    {
        VkResult result = VK_SUCCESS;
        bool acquired = false;
        bool recreateNeeded = false;
        uint32_t imageIndex = 0;
    };

    struct XJSwapchainPresentResult
    {
        VkResult result = VK_SUCCESS;
        bool presented = false;
        bool recreateNeeded = false;
    };

    class XJVulkanSwapchain
    {
        private:
            void  SetupSurfaceCapabilities();//设置交换链
            /* data */
            VkSwapchainKHR  mSwapchain = VK_NULL_HANDLE;
            std::vector<VkImage> mImages;//存储交换链里面的图片  

            XJVulkanPhysicalDevices* mPhysicalDevice;
            XJVulkanDevice* mDevice;
            XJVulkanSurface* mSurface;

            int32_t mCurrentImageIndex = -1;//当前图片索引

            SurfaceInfo mSurfaceInfo;

            VkExtent2D mExtent{0, 0};
            uint64_t mGeneration = 0;//每次成功创建一代新的 swapchain images 后递增

        public:
            XJVulkanSwapchain(XJVulkanPhysicalDevices* physicalDevice, XJVulkanDevice* device, XJVulkanSurface* surface);
            ~XJVulkanSwapchain();
            XJVulkanSwapchain(const XJVulkanSwapchain&) = delete;
            XJVulkanSwapchain& operator=(const XJVulkanSwapchain&) = delete;

            bool ReCreate();

            XJSwapchainAcquireResult AcquireImage( //获取图片
                VkSemaphore semaphore,
                VkFence fence = VK_NULL_HANDLE);

            XJSwapchainPresentResult Present(
                uint32_t imageIndex,
                const std::vector<VkSemaphore>& waitSemaphores);

            int32_t XJGetCurrentImageIndex() const { return mCurrentImageIndex; }

            VkSwapchainKHR XJGetVulkanSwapchain() { return mSwapchain; }

            const std::vector<VkImage>& XJGetSwapchainImages() const { return mImages; }
            uint32_t XJGetWidth() const { return mExtent.width; }
            uint32_t XJGetHeight() const { return mExtent.height; }
            uint64_t XJGetGeneration() const { return mGeneration; }

            const SurfaceInfo& XJGetSurfaceInfo() const { return mSurfaceInfo; }
            
    };
    

    
}

#endif
