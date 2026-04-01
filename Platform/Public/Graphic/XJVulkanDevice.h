#ifndef XJ_VULKAN_DEVICE_H
#define XJ_VULKAN_DEVICE_H

#include "Edit/EditIncludes.h"
#include "Graphic/VulkanCommon.h"
#include "Graphic/VulkanQueue.h"
//逻辑设备

namespace XJ
{   

    // class VulkanQueue;

    class XJVulkanCommandPool;
    class VulkanPhysicalDevices;

    struct VkSettings//创建逻辑设备的时候做存储
    {
        VkFormat surfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
        //VkFormat surfaceFormat = VK_FORMAT_B8G8R8_UNORM;
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT; // 深度格式
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        uint32_t swapchainImageCount = 3;
    };
    
    class XJVulkanDevice
    {
      
        public:
            XJVulkanDevice(VulkanPhysicalDevices* physicalDevices, uint32_t graphicsQueueIndex, uint32_t presentQueueIndex, const VkSettings &settings = {});
            // void DeviceInit(VulkanPhysicalDevices* physicalDevices);
            ~XJVulkanDevice();
            
            // 可选：提供获取Device的接口
            VkDevice XJGetDevice() { return mDevice; }
            VkSettings XJGetSettings() { return settings;}
            VkPipelineCache XJGetPipelineCache() const { return mPipelineCache; }

            VulkanQueue* XJGetGraphicQueue(uint32_t index) const { return mGraphicQueue.size() < index + 1 ? nullptr : mGraphicQueue[index].get(); };
            VulkanQueue* XJGetFirstGraphicQueue() const { return mGraphicQueue.empty() ? nullptr : mGraphicQueue[0].get(); };
            VulkanQueue* XJGetPresentQueue(uint32_t index) const { return mPresentQueue.size() < index + 1 ? nullptr : mPresentQueue[index].get(); };
            VulkanQueue* XJGetFirstPresentQueue() const { return mPresentQueue.empty() ? nullptr : mPresentQueue[0].get(); };
            XJVulkanCommandPool *XJGetDefaultCmdPool() const { return mDefaultCmdPool.get(); }


            int32_t XJGetMemoryIndex( VkMemoryPropertyFlags memoryPropertyFlags, uint32_t memoryTypeBits) const;
            VkCommandBuffer CreateAndBeginOneDefaultCommandBuffer();//开始单个命令缓冲区
            void SubmitAndEndOneDefaultCommandBuffer(VkCommandBuffer& commandBuffer);//结束单个命令缓冲区

           
        private:
            void CreatePipelineCache();//创建管线缓存
            void CreateDefaultCommandPool();//创建默认命令池

            VkDevice mDevice = VK_NULL_HANDLE;
            VulkanPhysicalDevices *mPhysicalDevices;

            std::vector<std::shared_ptr<VulkanQueue>> mGraphicQueue;
            std::vector<std::shared_ptr<VulkanQueue>> mPresentQueue;
            std::shared_ptr<XJVulkanCommandPool> mDefaultCmdPool;

            VkSettings settings;

            VkPipelineCache mPipelineCache = VK_NULL_HANDLE;
    }; 
    
}


#endif