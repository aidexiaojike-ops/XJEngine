#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanImageView.h"
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
        if (!mDevice || !mDevice->IsValid())
        {
            spdlog::error("XJVulkanFrameBuffer create failed: device is invalid");
            return;
        }

        if (!mRenderPass || mRenderPass->XJGetRenderPass() == VK_NULL_HANDLE)
        {
            spdlog::error("XJVulkanFrameBuffer create failed: render pass is invalid");
            return;
        }

        if (width == 0 || height == 0)
        {
            spdlog::error("XJVulkanFrameBuffer create failed: invalid extent {}x{}", width, height);
            return;
        }

        ReCreate(colorImages, depthImage, resolveImage, width, height);
    }
    
    XJVulkanFrameBuffer::~XJVulkanFrameBuffer()
    {
        if (!mDevice || !mDevice->IsValid())
        {
            return;
        }

        mDevice->WaitIdle();

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
        

        if (!mDevice || !mDevice->IsValid())
        {
            spdlog::error("FrameBuffer ReCreate failed: device is invalid");
            return false;
        }

        if (!mRenderPass || mRenderPass->XJGetRenderPass() == VK_NULL_HANDLE)
        {
            spdlog::error("FrameBuffer ReCreate failed: render pass is invalid");
            return false;
        }

        if (width == 0 || height == 0)
        {
            spdlog::error("FrameBuffer ReCreate failed: invalid extent {}x{}", width, height);
            return false;
        }
        std::vector<std::shared_ptr<XJVulkanImageView>> newColorViews;
        std::vector<std::shared_ptr<XJVulkanImageView>> newResolveViews;
        std::vector<VkImageView> attachments;// 准备附件数组（颜色附件 + 深度附件）

        newColorViews.reserve(colorImages.size());
         // 1. 添加颜色附件（交换链图像）
        for (size_t i = 0; i < colorImages.size(); ++i)
        {
            if (!colorImages[i] || !colorImages[i]->IsValid())
            {
                spdlog::error("FrameBuffer ReCreate failed: color image {} is invalid", i);
                return false;
            }

            auto colorView = std::make_shared<XJVulkanImageView>(
                mDevice,
                colorImages[i]->XJGetImage(),
                colorImages[i]->XJGetFormat(),
                VK_IMAGE_ASPECT_COLOR_BIT);

            if (!colorView || colorView->XJGetImageView() == VK_NULL_HANDLE)
            {
                spdlog::error("FrameBuffer ReCreate failed: color image view {} create failed", i);
                return false;
            }

            attachments.push_back(colorView->XJGetImageView());
            newColorViews.push_back(colorView);
        }
         // 2. 添加深度附件（如果存在）
        if (depthImage && depthImage->IsValid())
        {
            attachments.push_back(depthImage->XJGetImageView());
        }
        else
        {
            spdlog::warn(
                "FrameBuffer ReCreate/深度图像无效或缺失: depth image is invalid or missing, ptr={}, IsValid={}, image={}, view={}",
                (void*)depthImage.get(),
                depthImage ? static_cast<int>(depthImage->IsValid()) : -1,
                depthImage ? (void*)depthImage->XJGetImage() : nullptr,
                depthImage ? (void*)depthImage->XJGetImageView() : nullptr);
        }
        // 3. 添加解析附件（对应索引2）
        if (resolveImage && resolveImage->IsValid())
        {
            auto resolveView = std::make_shared<XJVulkanImageView>(
                mDevice,
                resolveImage->XJGetImage(),
                resolveImage->XJGetFormat(),
                VK_IMAGE_ASPECT_COLOR_BIT);

            if (!resolveView || resolveView->XJGetImageView() == VK_NULL_HANDLE)
            {
                spdlog::error("FrameBuffer ReCreate failed: resolve image view create failed");
                return false;
            }
            spdlog::debug("帧缓冲添加深度附件: {}", static_cast<void*>(resolveView->XJGetImageView()));
            attachments.push_back(resolveView->XJGetImageView());
            newResolveViews.push_back(resolveView);
        }

        if (attachments.empty())
        {
            spdlog::error("FrameBuffer ReCreate failed: no attachments");
            return false;
        }

        VkFramebuffer newFrameBuffer = VK_NULL_HANDLE;
        //创建帧缓冲
        VkFramebufferCreateInfo frameBufferInfo{};
        frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.pNext = nullptr;
        frameBufferInfo.flags = 0;
        frameBufferInfo.renderPass = mRenderPass->XJGetRenderPass();
        frameBufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        frameBufferInfo.pAttachments = attachments.data();
        frameBufferInfo.width = width;
        frameBufferInfo.height = height;
        frameBufferInfo.layers = 1;

        VkResult ret = vkCreateFramebuffer(mDevice->XJGetDevice(), &frameBufferInfo, nullptr, &newFrameBuffer);
        if (ret != VK_SUCCESS)
        {
            spdlog::error("创建帧缓冲失败: {}", vk_result_string(ret));
            return false;
        }

        VkFramebuffer oldFrameBuffer = mFrameBuffer;
        mFrameBuffer = newFrameBuffer;
        mWidth = width;
        mHeight = height;
        mColorImages = colorImages;
        mDepthImage = depthImage;
        mResolveImage = resolveImage;
        mColorViews = std::move(newColorViews);
        mResolveViews = std::move(newResolveViews);

        if (oldFrameBuffer != VK_NULL_HANDLE)
        {
            mDevice->WaitIdle();
            vkDestroyFramebuffer(mDevice->XJGetDevice(), oldFrameBuffer, nullptr);
        }

        
        spdlog::trace("函数来自: {0}, 创建帧缓冲结果：{1}, width: {2}, height: {3}, 附件数量: {4}", 
            __FUNCTION__, vk_result_string(ret), mWidth, mHeight, attachments.size());
        
        return true;
    }

}
