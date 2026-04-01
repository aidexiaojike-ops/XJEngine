#include "Render/XJTexture.h"
#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanImage.h"
#include "Graphic/VulkanImageView.h"
#include "Graphic/XJVulkanDevice.h"
#include "Render/XJRenderContext.h"
#include "XJApplication.h"
#include "Graphic/XJVulkanTextureSampler.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace XJ
{
    XJTexture::XJTexture(const std::string &filePath)
    {
        int numChannel;//贴图通道
        uint8_t *data =  stbi_load(filePath.c_str(), reinterpret_cast<int *>(&mWidth), reinterpret_cast<int *>(&mHeight), &numChannel, STBI_rgb_alpha);
        if(!data)
        {
            spdlog::error("Can not load this image: {0}", filePath);
            return;
        }

        mFormat = VK_FORMAT_R8G8B8A8_UNORM;
        XJ::XJRenderContext *renderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice  = renderContext->XJGetDevice();
        mImage = std::make_shared<XJVulkanImage>(kDevice,VkExtent3D{mWidth, mHeight, 1},
                 mFormat, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
        mImageView = std::make_shared<VulkanImageView>(kDevice,mImage->XJGetImage(), mFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        XJDebug_Log(mTextureSampler->CreateSimpleSampler(kDevice, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, &mSampler));
        
        //COPY DATA TO BUFFER
        size_t size = sizeof(uint8_t) * 4 * mWidth * mHeight;
        std::shared_ptr<XJVulkanBuffer> stageBuffer = std::make_shared<XJVulkanBuffer>(kDevice, 
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, data, true);
        //COPY BUFFER TO IMAGE
        VkCommandBuffer cmdBuffer = kDevice->CreateAndBeginOneDefaultCommandBuffer();
        XJVulkanImage::TransitionLayout(cmdBuffer, mImage->XJGetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);//复制buffer容器
        mImage->CopyFromBuffer(cmdBuffer, stageBuffer.get());
        XJVulkanImage::TransitionLayout(cmdBuffer, mImage->XJGetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);//释放复制容器
        kDevice->SubmitAndEndOneDefaultCommandBuffer(cmdBuffer);

        stageBuffer.reset();
        stbi_image_free(data);
    }
    XJTexture::~XJTexture()
    {
        XJ::XJRenderContext *renderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice  = renderContext->XJGetDevice();
        vkDestroySampler(kDevice->XJGetDevice(), mSampler, nullptr);
        mImage.reset();
        mImageView.reset();
    }
}