#ifndef XJ_VULKAN_IMAGEVIEW_H
#define XJ_VULKAN_IMAGEVIEW_H


#include "Graphic/VulkanCommon.h"

namespace XJ
{
    class XJVulkanDevice;

    class XJVulkanImageView
    {
        private:
            /* data */
            VkImageView mImageView = VK_NULL_HANDLE;
            XJVulkanDevice* mDevice = nullptr;

        public:
        //VkImageAspectFlags is a bitmask type to specify which aspect(s) of an image are included in a view.
            XJVulkanImageView(XJVulkanDevice* device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
            ~XJVulkanImageView();
            //禁止 禁用拷贝构造/拷贝赋值。任何意外按值传递/返回都会触发两次 vkDestroy* → 崩溃或驱动层错误。
            XJVulkanImageView(const XJVulkanImageView&) = delete;
            XJVulkanImageView& operator=(const XJVulkanImageView&) = delete;

            VkImageView XJGetImageView() const { return mImageView; }
    };
}
#endif
