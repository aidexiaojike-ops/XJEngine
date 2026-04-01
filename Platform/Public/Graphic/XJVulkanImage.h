#ifndef XJ_VULKAN_IMAGE_H
#define XJ_VULKAN_IMAGE_H

#include <Graphic/VulkanCommon.h>

namespace XJ
{
    class XJVulkanDevice;
    class XJVulkanBuffer;

    class XJVulkanImage
    {
        public:
            XJVulkanImage(XJVulkanDevice* device, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
            XJVulkanImage(XJVulkanDevice *device, VkImage image, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
                        
            void CopyFromBuffer(VkCommandBuffer cmdBuffer, XJVulkanBuffer *buffer);
            
            static bool TransitionLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

            ~XJVulkanImage();

            const VkImage& XJGetImage() const { return mImage; }
            const VkDeviceMemory& XJGetImageMemory() const { return mDeviceMemory; }
            const VkFormat& XJGetFormat() const { return mFormat; }

            bool IsValid() const { return mImage != VK_NULL_HANDLE; }
        private:
            VkImage mImage = VK_NULL_HANDLE;
            VkDeviceMemory mDeviceMemory = VK_NULL_HANDLE;

            bool bCreateImage = true;//是否创建了image

            
            XJVulkanDevice* mDevice;
            // 用于存储图像的格式、尺寸和用途
            VkFormat mFormat;
            VkExtent3D mExtent;
            VkImageUsageFlags mUsage;

            bool mIsSwapchainImage = false;  // 🔧 添加这个标志

            VkSampleCountFlagBits mSampleCount; // 多重采样数量
    };
    
}



#endif