#ifndef VULKAN_FRAMEBUFFER_H
#define VULKAN_FRAMEBUFFER_H

#include "Graphic/VulkanCommon.h"
#include "Graphic/VulkanImageView.h"
#include "Graphic/XJVulkanDepthImage.h"  

namespace XJ
{
    class XJVulkanDevice;
    class XJVulkanRenderPass;
    // class VulkanImageView;
    class XJVulkanImage;
    // class XJVulkanDepthImage;

    class XJVulkanFrameBuffer
    {
        private:
            /* data */
            VkFramebuffer mFrameBuffer = VK_NULL_HANDLE;
            XJVulkanDevice *mDevice = nullptr;
            XJVulkanRenderPass *mRenderPass = nullptr;
            uint32_t mWidth = 0;
            uint32_t mHeight = 0;

            std::vector<std::shared_ptr<VulkanImageView>> mColorViews;
            // std::vector<std::shared_ptr<VulkanImageView>> mDepthViews; // 新增：深度图像视图向量
            std::vector<std::shared_ptr<VulkanImageView>> mResolveViews;

            std::vector<std::shared_ptr<XJVulkanImage>> mColorImages;
            std::shared_ptr<XJVulkanDepthImage> mDepthImage;
            std::shared_ptr<XJVulkanImage> mResolveImage;

            //std::vector<std::shared_ptr<VulkanImageView>> mDepthImageViews; // 新增：深度图像视图

        public:
            // 构造函数现在接受颜色图像向量、深度图像指针、解析图像指针
            XJVulkanFrameBuffer(XJVulkanDevice *device, XJVulkanRenderPass *renderPass,
                           const std::vector<std::shared_ptr<XJVulkanImage>> &colorImages,
                           const std::shared_ptr<XJVulkanDepthImage>& depthImage,
                           const std::shared_ptr<XJVulkanImage>& resolveImage,
                           uint32_t width, uint32_t height);
            ~XJVulkanFrameBuffer();
            XJVulkanFrameBuffer(const XJVulkanFrameBuffer&) = delete;
            XJVulkanFrameBuffer& operator=(const XJVulkanFrameBuffer&) = delete;

            bool ReCreate(const std::vector<std::shared_ptr<XJVulkanImage>> &colorImages,
                     const std::shared_ptr<XJVulkanDepthImage>& depthImage,
                     const std::shared_ptr<XJVulkanImage>& resolveImage,
                     uint32_t width, uint32_t height);


            std::shared_ptr<VulkanImageView> XJGetColorImageView(uint32_t index = 0) const
            {
                return (index < mColorViews.size()) ? mColorViews[index] : nullptr;
            }
            VkImageView XJGetColorImageViewHandle(uint32_t index = 0) const
            {
                auto view = XJGetColorImageView(index);
                return view ? view->XJGetImageView() : VK_NULL_HANDLE;
            }

            VkFramebuffer XJGetFrameBuffer() const { return mFrameBuffer; }

            uint32_t XJGetWidth() const { return mWidth; }
            uint32_t XJGetHeight() const { return mHeight; }

            bool IsValid() const { return mFrameBuffer != VK_NULL_HANDLE; }
        
            // 获取附件数量（用于调试）
            size_t GetAttachmentCount() const { return mColorViews.size() + ((mDepthImage && mDepthImage->IsValid()) ? 1 : 0); }
    };
    

}
#endif