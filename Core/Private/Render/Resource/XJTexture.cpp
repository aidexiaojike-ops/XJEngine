#include "Render/Resource/XJTexture.h"
#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanImage.h"
#include "Graphic/XJVulkanImageView.h"
#include "Graphic/XJVulkanDevice.h"
#include "Render/XJRenderContext.h"
#include "XJApplication.h"
#include <spdlog/spdlog.h>
#include "Asset/Importer/XJTextureImporter.h"

#include <thread>

namespace XJ
{
    XJTexture::XJTexture(uint32_t width, uint32_t height, RGBAColor *pixels) : mWidth(width), mHeight(height) 
    {   
        // 设置纹理的格式为 VK_FORMAT_R8G8B8A8_UNORM（标准 RGBA 格式）
        mFormat = VK_FORMAT_R8G8B8A8_UNORM;//mipmap
        size_t size = sizeof(uint8_t) * 4 * mWidth * mHeight;
        CreateImage(size, pixels);
        
    }
    XJTexture::~XJTexture()
    {
        // ImageView 依赖 Image，先释放 view 再释放 image。
        // 不在析构里重新从全局 context 取 device，避免 Stop 后全局指针已清空时崩溃。
        mImageView.reset();
        mImage.reset();
    }

    void XJTexture::CreateImage(size_t size, void *data) 
    {
        XJAppContext* appContext = XJApplication::XJGetAppContext();
        if (!appContext)
        {
            spdlog::error("XJTexture::CreateImage failed: app context is null");
            return;
        }

        // 当前实现使用 XJVulkanDevice 默认 command pool/queue 上传纹理。
        // Vulkan command pool 和 queue 需要外部同步，所以 GPU 纹理创建必须在渲染线程执行。
        if (appContext->renderThreadId != std::thread::id{} &&
            appContext->renderThreadId != std::this_thread::get_id())
        {
            spdlog::error("XJTexture::CreateImage failed: texture GPU upload must run on render thread.");
            return;
        }

        // 线程约束：当前实现使用 XJVulkanDevice 的默认 command pool/queue 上传纹理。
        // Vulkan command pool 和 queue 需要外部同步，所以运行时纹理创建必须在渲染线程执行。
        // 真正的多线程资源加载应改成：工作线程只解析文件，GPU image/buffer 创建投递到渲染线程执行。
        XJ::XJRenderContext *kRenderCxt = appContext->renderContext;
        if (!kRenderCxt)
        {
            spdlog::error("XJTexture::CreateImage failed: render context is null");
            return;
        }

        XJ::XJVulkanDevice *kDevice = kRenderCxt->XJGetDevice();
        if (!kDevice || !kDevice->IsValid())
        {
            spdlog::error("XJTexture::CreateImage failed: device is invalid");
            return;
        }
        // 创建 Vulkan 图像（大小为 mWidth x mHeight，格式为 mFormat，包含两种用途：传输目标和可采样）
        mImage = std::make_shared<XJVulkanImage>(kDevice, VkExtent3D{ mWidth, mHeight, 1 }, 
                 mFormat, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                 VK_SAMPLE_COUNT_1_BIT);
        // 创建图像视图
        mImageView = std::make_shared<XJVulkanImageView>(kDevice, mImage->XJGetImage(), mFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        // copy data to buffer
        std::shared_ptr<XJVulkanBuffer> kStageBuffer = std::make_shared<XJVulkanBuffer>(kDevice, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, data, true);

        // copy buffer to image
         // UNDEFINED -> TRANSFER_DST -> copy -> SHADER_READ_ONLY_OPTIMAL
        // 将图像的布局转换为可进行传输的状态
        VkCommandBuffer cmdBuffer = kDevice->CreateAndBeginOneDefaultCommandBuffer();
        XJVulkanImage::TransitionLayout(
            cmdBuffer,
            mImage->XJGetImage(),
            mImage->XJGetFormat(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        // 将缓冲区中的数据拷贝到 Vulkan 图像
        mImage->CopyFromBuffer(cmdBuffer, kStageBuffer.get());
          // 将图像的布局转换为着色器只读状态
        XJVulkanImage::TransitionLayout(
            cmdBuffer,
            mImage->XJGetImage(),
            mImage->XJGetFormat(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
         // 提交命令缓冲区
        kDevice->SubmitAndEndOneDefaultCommandBuffer(cmdBuffer);
         // 清理缓冲区
        kStageBuffer.reset();
    }
}