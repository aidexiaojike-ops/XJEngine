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
            //禁止 禁用拷贝构造/拷贝赋值。任何意外按值传递/返回都会触发两次 vkDestroy* → 崩溃或驱动层错误。
            VulkanImageView(const VulkanImageView&) = delete;
            VulkanImageView& operator=(const VulkanImageView&) = delete;

            VkImageView XJGetImageView() const { return mImageView; }
    };
}
#endif