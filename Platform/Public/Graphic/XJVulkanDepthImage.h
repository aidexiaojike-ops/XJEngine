#ifndef XJ_VULKAN_DEPTH_IMAGE_H
#define XJ_VULKAN_DEPTH_IMAGE_H

#include "Graphic/VulkanCommon.h"
#include "Graphic/XJVulkanDevice.h"

namespace XJ
{
    class XJVulkanDepthImage
    {
        private:
            XJVulkanDevice* mDevice;
            VulkanPhysicalDevices* mPhysicalDevices;
            VkImage mDepthImage = VK_NULL_HANDLE;
            VkDeviceMemory mDepthImageMemory = VK_NULL_HANDLE;
            VkImageView mDepthImageView = VK_NULL_HANDLE;
            uint32_t mWidth = 0;
            uint32_t mHeight = 0;
            VkFormat mFormat = VK_FORMAT_UNDEFINED;

            VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT;

        public:
            XJVulkanDepthImage(XJVulkanDevice* device, VulkanPhysicalDevices* physicalDevice, uint32_t width, uint32_t height, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
            ~XJVulkanDepthImage();

            bool Create();
            void Destroy();

            VkImage XJGetImage() const { return mDepthImage; }
            VkImageView XJGetImageView() const { return mDepthImageView; }
            VkFormat XJGetFormat() const { return mFormat; }

            VkFormat FindDepthFormat() const;

            uint32_t XJGetWidth() const { return mWidth; }
            uint32_t XJGetHeight() const { return mHeight; }

            bool IsValid() const { return mDepthImage != VK_NULL_HANDLE && mDepthImageView != VK_NULL_HANDLE; }

        private:
            bool CreateImage();
            bool AllocateMemory();
            bool CreateImageView();
            bool HasStencilComponent(VkFormat format) const;
    };
}

#endif