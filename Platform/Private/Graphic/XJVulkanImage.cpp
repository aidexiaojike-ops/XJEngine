#include <Graphic/XJVulkanImage.h>
#include <Graphic/XJVulkanDevice.h>
#include <Graphic/VulkanCommon.h>
#include <Graphic/XJVulkanBuffer.h>

namespace XJ
{
    XJVulkanImage::XJVulkanImage(XJVulkanDevice* device, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits sampleCount)
        : mDevice(device), mFormat(format), mExtent(extent), mUsage(usage), mSampleCount(sampleCount)
    {
        //默认线性平铺  深度格式使用最优平铺
        VkImageTiling tiling = VK_IMAGE_TILING_LINEAR;
        //VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        bool isDepthStencilFormat = IsDepthStencilFormat(format);
        if(isDepthStencilFormat || sampleCount > VK_SAMPLE_COUNT_1_BIT)
        {
            tiling = VK_IMAGE_TILING_OPTIMAL;//深度格式使用最优平铺
        }
        

        VkImageCreateInfo imageCreateInfo{};//结构体变量初始化 图片创建信息
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext = nullptr;
        imageCreateInfo.flags = 0;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;//二维图像  图片类型
        imageCreateInfo.format = format;//图片的格式
        imageCreateInfo.extent = {extent.width, extent.height, extent.depth};//图片的尺寸  宽高深度
        imageCreateInfo.mipLevels = 1;//mip 级别 数量
        imageCreateInfo.arrayLayers = 1;//数组层数
        imageCreateInfo.samples = sampleCount;//采样数
        imageCreateInfo.tiling = tiling;//图像的平铺方式  最优的
        imageCreateInfo.usage = usage;//图像的用途  深度附加使用深度用途
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;//独占模式
        imageCreateInfo.queueFamilyIndexCount = 0;//队列族索引数量
        imageCreateInfo.pQueueFamilyIndices = nullptr;//队列族索引指针
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//初始布局 未定义


        XJDebug_Log(vkCreateImage(mDevice->XJGetDevice(), &imageCreateInfo, nullptr, &mImage));

        VkMemoryRequirements memRequirements;//内存需求
        vkGetImageMemoryRequirements(mDevice->XJGetDevice(), mImage, &memRequirements);//获取图片内存需求 
        //创建图片 后续还要分配内存绑定图片
        VkMemoryAllocateInfo memoryAllocateInfo{};//结构体变量初始化 内存分配信息
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = nullptr;
        memoryAllocateInfo.allocationSize = memRequirements.size;//分配大小
        memoryAllocateInfo.memoryTypeIndex = static_cast<uint32_t>(mDevice->XJGetMemoryIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements.memoryTypeBits));
        
        XJDebug_Log(vkAllocateMemory(mDevice->XJGetDevice(), &memoryAllocateInfo, nullptr, &mDeviceMemory));//分配内存
        vkBindImageMemory(mDevice->XJGetDevice(), mImage, mDeviceMemory, 0);//绑定图片内存  偏移量0
    }


   XJVulkanImage::XJVulkanImage(XJVulkanDevice *device, VkImage image, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits sampleCount)
                            : mDevice(device), mImage(image), mExtent(extent), mFormat(format), mUsage(usage), mSampleCount(sampleCount), bCreateImage(false) 
                            {
    }


    XJVulkanImage::~XJVulkanImage()
    {
        if (mImage != VK_NULL_HANDLE && bCreateImage) // 只销毁自己创建的图像
        {
            vkDestroyImage(mDevice->XJGetDevice(), mImage, nullptr);
            mImage = VK_NULL_HANDLE;
        }
        if(mDeviceMemory != VK_NULL_HANDLE)// 内存总是自己分配的
        {
            vkFreeMemory(mDevice->XJGetDevice(), mDeviceMemory, nullptr);
            mDeviceMemory = VK_NULL_HANDLE;
        }
        
    } 
    void XJVulkanImage::CopyFromBuffer(VkCommandBuffer cmdBuffer, XJVulkanBuffer *buffer)
    {
        VkBufferImageCopy region{};//COPY 的范围
        region.bufferOffset = 0;
        region.bufferRowLength = mExtent.width;
        region.bufferImageHeight = mExtent.height;
        region.imageSubresource = 
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        region.imageOffset = {0,0,0};
        region.imageExtent = {mExtent.width, mExtent.height, 1};
        vkCmdCopyBufferToImage(cmdBuffer, buffer->XJGetBuffer(), mImage,
             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    bool XJVulkanImage::TransitionLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        if(image == VK_NULL_HANDLE){return false;}
        if(oldLayout == newLayout){return true;}

        VkImageMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        // Source layouts (old)
        // The source access mask controls actions to be finished on the old
        // layout before it will be transitioned to the new layout.

        switch (oldLayout) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                // Image layout is undefined (or does not matter).
                // Only valid as initial layout. No flags required.
                barrier.srcAccessMask = 0;
                break;
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                // Image is preinitialized.
                // Only valid as initial layout for linear images; preserves memory
                // contents. Make sure host writes have finished.
                barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image is a color attachment.
                // Make sure writes to the color buffer have finished
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image is a depth/stencil attachment.
                // Make sure any writes to the depth/stencil buffer have finished.
                barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image is a transfer source.
                // Make sure any reads from the image have finished
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image is a transfer destination.
                // Make sure any writes to the image have finished.
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image is read by a shader.
                // Make sure any shader reads from the image have finished
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                spdlog::error("Unsupported layout transition : {0} --> {1}", vk_image_layout_string(oldLayout), vk_image_layout_string(newLayout));
                return false;
        }

        // Target layouts (new)
        // The destination access mask controls the dependency for the new image
        // layout.
        switch (newLayout) {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image will be used as a transfer destination.
                // Make sure any writes to the image have finished.
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image will be used as a transfer source.
                // Make sure any reads from and writes to the image have finished.
                barrier.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image will be used as a color attachment.
                // Make sure any writes to the color buffer have finished.
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image layout will be used as a depth/stencil attachment.
                // Make sure any writes to depth/stencil buffer have finished.
                barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image will be read in a shader (sampler, input attachment).
                // Make sure any writes to the image have finished.
                if (barrier.srcAccessMask == 0)
                {
                    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                spdlog::error("Unsupported layout transition : {0} --> {1}", vk_image_layout_string(oldLayout), vk_image_layout_string(newLayout));
                return false;
        }

        vkCmdPipelineBarrier( cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        return true;

    }

}