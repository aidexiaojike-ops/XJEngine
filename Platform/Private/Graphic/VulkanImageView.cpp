#include "Graphic/VulkanImageView.h"
#include "Graphic/XJVulkanDevice.h"

namespace XJ
{
    VulkanImageView::VulkanImageView(XJVulkanDevice* device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
        : mDevice(device)
    {
        VkImageViewCreateInfo imageViewInfo = {};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.pNext = nullptr;
        imageViewInfo.flags = 0;
        imageViewInfo.image = image;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;//目前只支持2D   viewType是视图类型
        imageViewInfo.format = format;
        //组件映射  把图像的rgba 分别映射到 视图的rgba上
        imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY; 
        imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        //子资源范围  mipmap层级  数组层级  图像的哪一部分被包含在视图中
        imageViewInfo.subresourceRange.aspectMask = aspectFlags;//指定图像的哪些方面包含在视图中
        imageViewInfo.subresourceRange.baseMipLevel = 0;//指定视图的第一个mipmap级别
        imageViewInfo.subresourceRange.levelCount = 1;//指定视图中的mipmap级别数量
        imageViewInfo.subresourceRange.baseArrayLayer = 0;//指定视图的第一个数组层
        imageViewInfo.subresourceRange.layerCount = 1;//指定视图中的数组层数

        XJDebug_Log(vkCreateImageView(mDevice->XJGetDevice(), &imageViewInfo, nullptr, &mImageView));

    }

    VulkanImageView::~VulkanImageView()
    {
        vkDestroyImageView(mDevice->XJGetDevice(), mImageView, nullptr);
    }
}