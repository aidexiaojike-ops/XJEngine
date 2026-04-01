#ifndef VULKAN_IMAGEVIEW_H
#define VULKAN_IMAGEVIEW_H


#include "Graphic/VulkanCommon.h"

namespace XJ
{
    class XJVulkanDevice;

    class VulkanImageView
    {
        private:
            /* data */
            VkImageView mImageView = VK_NULL_HANDLE;
            XJVulkanDevice* mDevice = nullptr;

        public:
        //VkImageAspectFlags is a bitmask type to specify which aspect(s) of an image are included in a view.
            VulkanImageView(XJVulkanDevice* device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
            ~VulkanImageView();

            VkImageView XJGetImageView() const { return mImageView; }
    };
}
#endif