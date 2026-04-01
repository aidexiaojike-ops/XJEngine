#include "Graphic/XJVulkanDepthImage.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/VulkanPhysicalDevices.h"

namespace XJ
{
    XJVulkanDepthImage::XJVulkanDepthImage(XJVulkanDevice* device, VulkanPhysicalDevices* physicalDevice, uint32_t width, uint32_t height, VkSampleCountFlagBits samples)
        : mDevice(device), mPhysicalDevices(physicalDevice), mWidth(width), mHeight(height),mSampleCount(samples) 
    {
        mFormat = FindDepthFormat();
        spdlog::debug("创建深度图像: {}x{}, 格式: {}", width, height, vk_format_string(mFormat));
    }
    
    XJVulkanDepthImage::~XJVulkanDepthImage()
    {
        Destroy();
    }
    
    bool XJVulkanDepthImage::Create()
    {
        if (!CreateImage()) {
            spdlog::error("创建深度图像失败");
            return false;
        }
        
        if (!AllocateMemory()) {
            spdlog::error("分配深度图像内存失败");
            return false;
        }
        
        if (!CreateImageView()) {
            spdlog::error("创建深度图像视图失败");
            return false;
        }
        
        spdlog::debug("深度图像创建成功");
        return true;
    }
    // 销毁深度图像资源
    void XJVulkanDepthImage::Destroy()
    {
        if (mDevice && mDevice->XJGetDevice() != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(mDevice->XJGetDevice());
            
            if (mDepthImageView != VK_NULL_HANDLE) {
                vkDestroyImageView(mDevice->XJGetDevice(), mDepthImageView, nullptr);
                mDepthImageView = VK_NULL_HANDLE;
            }
            
            if (mDepthImage != VK_NULL_HANDLE) {
                vkDestroyImage(mDevice->XJGetDevice(), mDepthImage, nullptr);
                mDepthImage = VK_NULL_HANDLE;
            }
            
            if (mDepthImageMemory != VK_NULL_HANDLE) {
                vkFreeMemory(mDevice->XJGetDevice(), mDepthImageMemory, nullptr);
                mDepthImageMemory = VK_NULL_HANDLE;
            }
        }
    }
    // 查找支持的深度格式
    VkFormat XJVulkanDepthImage::FindDepthFormat() const
    {
        std::vector<VkFormat> candidates = {
            VK_FORMAT_D32_SFLOAT,   // 优先使用 D32_SFLOAT
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM
        };
        
        for (VkFormat format : candidates) 
        {
            VkFormatProperties props;
            //vkGetPhysicalDeviceFormatProperties(mDevice->XJGetDevice(), format, &props);
            vkGetPhysicalDeviceFormatProperties(mPhysicalDevices->XJGetPhysicalDevice(), format, &props);
            
            
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                return format;
            }
        }
        
        spdlog::error("未找到支持的深度格式");
        return VK_FORMAT_UNDEFINED;
    }
    // 创建深度图像
    bool XJVulkanDepthImage::CreateImage()
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = mWidth;
        imageInfo.extent.height = mHeight;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = mFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = mSampleCount;
        imageInfo.flags = 0;
        
        VkResult result = vkCreateImage(mDevice->XJGetDevice(), &imageInfo, nullptr, &mDepthImage);
        if (result != VK_SUCCESS) {
            spdlog::error("创建深度图像失败: {}", vk_result_string(result));
            return false;
        }
        
        return true;
    }
    // 分配深度图像内存并绑定
    bool XJVulkanDepthImage::AllocateMemory()
    {
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(mDevice->XJGetDevice(), mDepthImage, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = mDevice->XJGetMemoryIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements.memoryTypeBits);
        //allocInfo.memoryTypeIndex = mDevice->XJGetMemoryIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements.memoryTypeBits);
        
        
        if (allocInfo.memoryTypeIndex == -1) {
            spdlog::error("找不到合适的深度图像内存类型");
            return false;
        }
        
        VkResult result = vkAllocateMemory(mDevice->XJGetDevice(), &allocInfo, nullptr, &mDepthImageMemory);
        if (result != VK_SUCCESS) {
            spdlog::error("分配深度图像内存失败: {}", vk_result_string(result));
            return false;
        }
        
        result = vkBindImageMemory(mDevice->XJGetDevice(), mDepthImage, mDepthImageMemory, 0);
        if (result != VK_SUCCESS) {
            spdlog::error("绑定深度图像内存失败: {}", vk_result_string(result));
            return false;
        }
        
        return true;
    }
    // 创建深度图像视图
    bool XJVulkanDepthImage::CreateImageView()
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = mDepthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = mFormat;
        viewInfo.subresourceRange.aspectMask = HasStencilComponent(mFormat) 
            ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
            : VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        VkResult result = vkCreateImageView(mDevice->XJGetDevice(), &viewInfo, nullptr, &mDepthImageView);
        if (result != VK_SUCCESS) {
            spdlog::error("创建深度图像视图失败: {}", vk_result_string(result));
            return false;
        }
        
        return true;
    }
    // 检查格式是否包含模板组件
    bool XJVulkanDepthImage::HasStencilComponent(VkFormat format) const
    {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || 
               format == VK_FORMAT_D24_UNORM_S8_UINT ||
               format == VK_FORMAT_D16_UNORM_S8_UINT;
    }
}