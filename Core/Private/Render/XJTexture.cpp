#include "Render/XJTexture.h"
#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanImage.h"
#include "Graphic/VulkanImageView.h"
#include "Graphic/XJVulkanDevice.h"
#include "Render/XJRenderContext.h"
#include "XJApplication.h"
#

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace XJ
{
    XJTexture::XJTexture(const std::string &filePath)
    {
        int numChannel;//贴图通道
         // 加载图片数据，STBI_rgb_alpha 表示加载为 RGBA 格式
        uint8_t *data =  stbi_load(filePath.c_str(), reinterpret_cast<int *>(&mWidth), reinterpret_cast<int *>(&mHeight), &numChannel, STBI_rgb_alpha);
        if(!data)
        {
            spdlog::error("Can not load this image: {0}", filePath);
            return;
        }

        mFormat = VK_FORMAT_R8G8B8A8_UNORM;
        size_t size = sizeof(uint8_t) * 4 * mWidth * mHeight;  // 计算图片的总大小（RGBA）
        //清理图片
        CreateImage(size, data);
        // 释放图片数据
        stbi_image_free(data);
    }
    XJTexture::XJTexture(uint32_t width, uint32_t height, RGBAColor *pixels) : mWidth(width), mHeight(height) 
    {   
        // 设置纹理的格式为 VK_FORMAT_R8G8B8A8_UNORM（标准 RGBA 格式）
        mFormat = VK_FORMAT_R8G8B8A8_UNORM;//mipmap
        size_t size = sizeof(uint8_t) * 4 * mWidth * mHeight;
        CreateImage(size, pixels);
    }
    XJTexture::~XJTexture()
    {
        XJ::XJRenderContext *renderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice  = renderContext->XJGetDevice();
        // vkDestroySampler(kDevice->XJGetDevice(), mSampler, nullptr);
        mImage.reset();
        mImageView.reset();
    }

    void XJTexture::CreateImage(size_t size, void *data) 
    {
         // 获取渲染上下文和设备
        XJ::XJRenderContext *kRenderCxt = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice = kRenderCxt->XJGetDevice();
        // 创建 Vulkan 图像（大小为 mWidth x mHeight，格式为 mFormat，包含两种用途：传输目标和可采样）
        mImage = std::make_shared<XJVulkanImage>(kDevice, VkExtent3D{ mWidth, mHeight, 1 }, 
                 mFormat, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                 VK_SAMPLE_COUNT_1_BIT);
        // 创建图像视图
        mImageView = std::make_shared<VulkanImageView>(kDevice, mImage->XJGetImage(), mFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        // copy data to buffer
        std::shared_ptr<XJVulkanBuffer> kStageBuffer = std::make_shared<XJVulkanBuffer>(kDevice, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, data, true);

        // copy buffer to image
         // UNDEFINED -> TRANSFER_DST -> copy -> SHADER_READ_ONLY_OPTIMAL
        // 将图像的布局转换为可进行传输的状态
        VkCommandBuffer cmdBuffer = kDevice->CreateAndBeginOneDefaultCommandBuffer();
        XJVulkanImage::TransitionLayout(cmdBuffer, mImage->XJGetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        // 将缓冲区中的数据拷贝到 Vulkan 图像
        mImage->CopyFromBuffer(cmdBuffer, kStageBuffer.get());
          // 将图像的布局转换为着色器只读状态
        XJVulkanImage::TransitionLayout(cmdBuffer, mImage->XJGetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
         // 提交命令缓冲区
        kDevice->SubmitAndEndOneDefaultCommandBuffer(cmdBuffer);
         // 清理缓冲区
        kStageBuffer.reset();
    }
}