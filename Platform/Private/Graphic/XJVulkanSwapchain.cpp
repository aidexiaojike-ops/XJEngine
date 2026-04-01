#include "Graphic/XJVulkanSwapchain.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/VulkanPhysicalDevices.h"
#include "Graphic/VulkanSurface.h"
#include "Graphic/VulkanQueue.h"
/*交换链 内部是几张图片 运行的时候从交换链里面取一张图片  把图片当作画布在里面绘制想要的内容（绘制过场叫渲染） 
**绘制完后（渲染完成）把图片还给交换链    交换链把提交过来的图片给显示（surface window）  这个过场叫呈现 present
**做图像的存储
**还有一种是 离屏渲染  不从交换链里面拿图片 从其他地方拿图片，然后绘制，再还回去，这个过场叫帧缓冲
**帧缓冲前面有render pass 是把一帧的渲染分成很多pass 比如阴影计算  光照计算 点击选中
*/
namespace XJ
{
    XJVulkanSwapchain::XJVulkanSwapchain(VulkanPhysicalDevices* physicalDevice, XJVulkanDevice* device, VulkanSurface* surface) 
    : mPhysicalDevice(physicalDevice), mDevice(device), mSurface(surface)
    {
        ReCreate();
    }
    
    XJVulkanSwapchain::~XJVulkanSwapchain()
    {
        if (mSwapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(mDevice->XJGetDevice(), mSwapchain, nullptr);
            mSwapchain = VK_NULL_HANDLE;
        }
    }
    bool XJVulkanSwapchain::ReCreate()
    {
        SetupSurfaceCapabilities();
        spdlog::debug("当前范围：{0} x {1}", mSurfaceInfo.capabilities.currentExtent.width, mSurfaceInfo.capabilities.currentExtent.height);
        spdlog::debug("表面格式：{0}", mSurfaceInfo.surfaceFormat.format);
        spdlog::debug("显示模式：{0}", mSurfaceInfo.presentMode);

        uint32_t imageCount = mDevice->XJGetSettings().swapchainImageCount;//图片数量 默认是3
        if(imageCount < mSurfaceInfo.capabilities.minImageCount && mSurfaceInfo.capabilities.minImageCount > 0)
        {
            imageCount = mSurfaceInfo.capabilities.minImageCount;
        }
        if(imageCount > mSurfaceInfo.capabilities.maxImageCount && mSurfaceInfo.capabilities.maxImageCount > 0)
        {
            imageCount = mSurfaceInfo.capabilities.maxImageCount;
        }
        //判断逻辑设备和物理设备是否是一个队列组
        VkSharingMode imageSharingMode;
        uint32_t queueFamilyIndexCount;
        uint32_t pQueueFamilyIndices[2] = {0, 0};
        if(mPhysicalDevice->isSameGraphicAndPresentQueueFamily())
        {
            imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            queueFamilyIndexCount = 0;
        }
        else
        {
            imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            queueFamilyIndexCount = 2;
            pQueueFamilyIndices[0] = mPhysicalDevice->XJGetGraphicQueueFamilyInfo().queueFamilyIndex;
            pQueueFamilyIndices[1] = mPhysicalDevice->XJGetGraphicQueueFamilyInfo().queueFamilyIndex;
        }

        VkSwapchainKHR oldSwapchain = mSwapchain;//存储原来的交换链

        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.pNext = nullptr;
        swapchainInfo.flags = 0;
        swapchainInfo.surface = mSurface -> XJGetSurface();
        swapchainInfo.minImageCount = imageCount;
        swapchainInfo.imageFormat = mSurfaceInfo.surfaceFormat.format;
        swapchainInfo.imageColorSpace =  mSurfaceInfo.surfaceFormat.colorSpace;
        swapchainInfo.imageExtent =  mSurfaceInfo.capabilities.currentExtent;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainInfo.imageSharingMode = imageSharingMode;
        swapchainInfo.queueFamilyIndexCount = queueFamilyIndexCount;
        swapchainInfo.pQueueFamilyIndices = pQueueFamilyIndices;
        //swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchainInfo.preTransform = mSurfaceInfo.capabilities.currentTransform;
        // 使用支持的compositeAlpha值
        //swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = mSurfaceInfo.presentMode;
        swapchainInfo.clipped = VK_FALSE;
        swapchainInfo.oldSwapchain = oldSwapchain;
  
        VkResult ret = vkCreateSwapchainKHR(mDevice->XJGetDevice(), &swapchainInfo, nullptr, &mSwapchain);
        if(ret != VK_SUCCESS)
        {
            spdlog::error("{0},{1}",__FUNCTION__, vk_result_string(ret));
            return false;
        }
        spdlog::trace("交换链 {0}:old:{1},new:{2},image count:{3}, format:{4}, present mode:{5}",
            __FUNCTION__,(void*)oldSwapchain,(void*)mSwapchain,imageCount,
            vk_format_string(mSurfaceInfo.surfaceFormat.format), vk_present_mode_string(mSurfaceInfo.presentMode));
        //获取到交换链里面的内容之后
        uint32_t swapchainImageCount;
        ret = vkGetSwapchainImagesKHR(mDevice->XJGetDevice(), mSwapchain, &swapchainImageCount, nullptr);
        mImages.resize(swapchainImageCount);
        ret = vkGetSwapchainImagesKHR(mDevice->XJGetDevice(), mSwapchain, &swapchainImageCount, mImages.data());

        return ret == VK_SUCCESS;
    }
    void XJVulkanSwapchain::SetupSurfaceCapabilities()
    {

        VkSettings settings = mDevice->XJGetSettings();
        //表面性能参数
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice->XJGetPhysicalDevice(), mSurface->XJGetSurface(), &mSurfaceInfo.capabilities);
        //surface 支持什么格式，选择最好的格式
        uint32_t formatCount;
        XJDebug_Log(vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice->XJGetPhysicalDevice(), mSurface->XJGetSurface(), &formatCount, nullptr));
        if(formatCount == 0)
        {
            spdlog::trace("{0}: 表面格式为 0", __FILE__);
        }
        std::vector<VkSurfaceFormatKHR> formats(formatCount);// 获取所有支持的表面格式
        XJDebug_Log(vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice->XJGetPhysicalDevice(), mSurface->XJGetSurface(), &formatCount, formats.data()));
        int32_t foundFormatIndex = -1;// 查找与设置中的格式匹配的表面格式
        for(int i = 0; i< formatCount; i++)
        {
            if(formats[i].format == settings.surfaceFormat && formats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
            {
                foundFormatIndex = i;
                break;
            }
        }
        //如果没找到 就用第一个
        if(foundFormatIndex == -1)
        {
            foundFormatIndex = 0;
        }
        mSurfaceInfo.surfaceFormat = formats[foundFormatIndex];

        // present mode 当前格式
        uint32_t presentModeCount;//查询surface 支持presentmode有那些
        XJDebug_Log(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice->XJGetPhysicalDevice(), mSurface->XJGetSurface(), &presentModeCount, nullptr));
        if(presentModeCount == 0)
        {
            spdlog::error("{0}: 显示模式数量为 0", __FILE__);
        }
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        XJDebug_Log(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice->XJGetPhysicalDevice(), mSurface->XJGetSurface(), &presentModeCount, presentModes.data()));
        int32_t foundPresentModeIndex = -1;
        for(int i = 0; i < presentModeCount; i++)//判断是否 是我们想要presentmode
        {
            if(presentModes[i] == settings.presentMode)
            {
                foundPresentModeIndex = i;
                break;
            }
        }
        if(foundPresentModeIndex == -1)
        {
            foundPresentModeIndex = 0;
        }
        mSurfaceInfo.presentMode = presentModes[foundPresentModeIndex];

    }
    VkResult XJVulkanSwapchain::AcquireImage(int32_t *outImageIndex,VkSemaphore semaphore, VkFence fence) 
    {
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(mDevice->XJGetDevice(), mSwapchain, UINT64_MAX, semaphore, fence, &imageIndex);
        //if(fence != VK_NULL_HANDLE)//如果有围栏的话 等待围栏完成 然后重置围栏
        //{
        //   XJDebug_Log(vkWaitForFences(mDevice->XJGetDevice(), 1, &fence, VK_TRUE, UINT64_MAX));
        //   XJDebug_Log(vkResetFences(mDevice->XJGetDevice(), 1, &fence));
        //}
        if(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
        {
            *outImageIndex = imageIndex;
            mCurrentImageIndex = imageIndex;
        }
        else
        {
            spdlog::error("{0}: 获取交换链图片失败，错误码：{1}", __FUNCTION__, vk_result_string(result));
        }
        return result;
    }

    VkResult XJVulkanSwapchain::Present(int32_t imageIndex, const std::vector<VkSemaphore>& waitSemaphores) 
    {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        presentInfo.pWaitSemaphores = waitSemaphores.data();
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &mSwapchain;
        presentInfo.pImageIndices = reinterpret_cast<const uint32_t*>(&imageIndex);
        presentInfo.pResults = nullptr;

        VkResult result = vkQueuePresentKHR(mDevice->XJGetFirstPresentQueue()->XJGetQueue(), &presentInfo);
        mDevice->XJGetFirstPresentQueue()->WaitIdle();
        return result;
    }

  
} 