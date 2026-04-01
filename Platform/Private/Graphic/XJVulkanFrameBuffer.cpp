#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/VulkanImageView.h"
#include "Graphic/XJVulkanImage.h"
#include "Graphic/XJVulkanDepthImage.h"

namespace XJ
{
    XJVulkanFrameBuffer::XJVulkanFrameBuffer(XJVulkanDevice *device, XJVulkanRenderPass *renderPass,
                                             const std::vector<std::shared_ptr<XJVulkanImage>> &colorImages,
                                             const std::shared_ptr<XJVulkanDepthImage>& depthImage,
                                             const std::shared_ptr<XJVulkanImage>& resolveImage,
                                             uint32_t width, uint32_t height)
        : mDevice(device), mRenderPass(renderPass), mDepthImage(depthImage), mResolveImage(resolveImage), mWidth(width), mHeight(height)
    {
        ReCreate(colorImages, depthImage, resolveImage, width, height);
    }
    
    XJVulkanFrameBuffer::~XJVulkanFrameBuffer()
    {
        if (mFrameBuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(mDevice->XJGetDevice(), mFrameBuffer, nullptr);
            mFrameBuffer = VK_NULL_HANDLE;
            //spdlog::trace("{0} : 销毁 帧缓冲 实例 : {1}", __FUNCTION__, (void*)mFrameBuffer);
        }
    }
    //  重新创建帧缓冲
    bool XJVulkanFrameBuffer::ReCreate(const std::vector<std::shared_ptr<XJVulkanImage>> &colorImages,
                                       const std::shared_ptr<XJVulkanDepthImage>& depthImage,
                                       const std::shared_ptr<XJVulkanImage>& resolveImage,
                                       uint32_t width, uint32_t height)
    {
        

        // 销毁旧的帧缓冲
        if(mFrameBuffer != VK_NULL_HANDLE && mDevice && mDevice->XJGetDevice() != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(mDevice->XJGetDevice(), mFrameBuffer, nullptr);
            mFrameBuffer = VK_NULL_HANDLE;
        }

        mWidth = width;
        mHeight = height;
        mColorImages = colorImages;
        mDepthImage = depthImage;
        mResolveImage = resolveImage;
        mColorViews.clear();
        mDepthViews.clear();
        mResolveViews.clear();
        // 准备附件数组（颜色附件 + 深度附件）
        std::vector<VkImageView> attachments;

         // 1. 添加颜色附件（交换链图像）
        for (size_t  i = 0; i < mColorImages.size(); i++)
        {
            if (!mColorImages[i] || !mColorImages[i]->IsValid()) 
            {
                spdlog::error("颜色图像无效");
                return false;
            }
       
            auto colorView = std::make_shared<VulkanImageView>(mDevice, 
                             mColorImages[i]->XJGetImage(), 
                             mColorImages[i]->XJGetFormat(), 
                             VK_IMAGE_ASPECT_COLOR_BIT);
            if (!colorView->XJGetImageView()) 
            {
            spdlog::error("颜色图像视图创建失败");
            return false;
            }
            mColorViews.push_back(colorView);
            attachments.push_back(mColorViews[i]->XJGetImageView());
        }

        // 2. 添加深度附件（如果提供了深度图像）
        if (mDepthImage && mDepthImage->IsValid())
        {
             // 深度图像视图已经由VulkanDepthImage创建，直接使用
            auto depthView = std::make_shared<VulkanImageView>(mDevice,
                mDepthImage->XJGetImage(),
                mDepthImage->XJGetFormat(),
                VK_IMAGE_ASPECT_DEPTH_BIT
            );
            mDepthViews.push_back(depthView);
            attachments.push_back(depthView->XJGetImageView());
        }
        else
        {
            spdlog::warn("深度图像无效或缺失，跳过深度附件");
        }
       
        // 3. 添加解析附件（对应索引2）
        if (mResolveImage && mResolveImage->IsValid())
        {
            auto resolveView = std::make_shared<VulkanImageView>
            (
                mDevice,
                mResolveImage->XJGetImage(),
                mResolveImage->XJGetFormat(),
                VK_IMAGE_ASPECT_COLOR_BIT
            );
            mResolveViews.push_back(resolveView);
            attachments.push_back(resolveView->XJGetImageView());
            spdlog::debug("帧缓冲添加深度附件: {}", static_cast<void*>(mDepthImage->XJGetImageView()));
        }
       
        
        spdlog::debug("帧缓冲附件：颜色={}, 深度={}, 解析={}", 
              mColorViews.size(), mDepthViews.size(), mResolveViews.size());
        for (size_t i = 0; i < attachments.size(); i++) {
            spdlog::debug("  附件[{}]: {}", i, (void*)attachments[i]);
        }
   
        spdlog::debug("创建帧缓冲，总附件数量: {}", attachments.size());

        //创建帧缓冲
        VkFramebufferCreateInfo frameBufferInfo = {};
        frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.pNext = nullptr;
        frameBufferInfo.flags = 0;
        frameBufferInfo.renderPass = mRenderPass->XJGetRenderPass();
        frameBufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        frameBufferInfo.pAttachments = attachments.data();
        frameBufferInfo.width = mWidth; 
        frameBufferInfo.height = mHeight;
        frameBufferInfo.layers = 1;

        VkResult ret = vkCreateFramebuffer(mDevice->XJGetDevice(), &frameBufferInfo, nullptr, &mFrameBuffer);
        if (ret != VK_SUCCESS)
        {
            spdlog::error("创建帧缓冲失败: {}", vk_result_string(ret));
            return false;
        }
        
        spdlog::trace("函数来自: {0}, 创建帧缓冲结果：{1}, width: {2}, height: {3}, 附件数量: {4}", 
            __FUNCTION__, vk_result_string(ret), mWidth, mHeight, attachments.size());
        
        return true;
    }

}