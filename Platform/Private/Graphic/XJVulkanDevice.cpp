#include "Graphic/XJVulkanDevice.h"
#include "Graphic/VulkanCommon.h"
#include "Graphic/VulkanQueue.h"
#include "Graphic/VulkanPhysicalDevices.h"
#include "Graphic/XJVulkanCommandBuffer.h"

namespace XJ
{
    const DeviceFeature requestedExtensions[] = 
    {
        //{ VK_KHR_SURFACE_EXTENSION_NAME, true },    // 所有平台都需要此扩展
        { VK_KHR_SWAPCHAIN_EXTENSION_NAME, true }
        //{ "VK_KHR_portability_subset", true}
    };
    XJ::XJVulkanDevice::XJVulkanDevice(VulkanPhysicalDevices* physicalDevices, uint32_t graphicsQueueIndex, uint32_t presentQueueIndex, const VkSettings &settings) 
    :settings(settings), mPhysicalDevices(physicalDevices)
    {
        if(!physicalDevices)
        {
            spdlog::error("XJVulkanDevice::XJVulkanDevice 参数错误，physicalDevices 为空指针");
            return;
        }
        QueueFamilyInfo graphicsQueueFamilyInfo = physicalDevices->XJGetGraphicQueueFamilyInfo();
        QueueFamilyInfo presentQueueFamilyInfo = physicalDevices->XJGetPresentQueueFamilyInfo();
        VkPhysicalDevice physicalDevice = physicalDevices->XJGetPhysicalDevice();
        //配置队列创建信息
        if(graphicsQueueIndex >= graphicsQueueFamilyInfo.queueCount)
        {
            spdlog::error("VulkanDevice::DeviceInit 参数错误，graphicsQueueIndex 超出范围");
            return;
        }
        if(presentQueueIndex >= presentQueueFamilyInfo.queueCount)
        {
            spdlog::error("VulkanDevice::DeviceInit 参数错误，presentQueueIndex 超出范围");
            return;
        }
        //队列优先级，范围0.0到1.0，1.0为最高优先级  显示队列优先级高于图形队列
        std::vector<float> graphicsQueuePriorities(graphicsQueueFamilyInfo.queueCount, 0.0f);//图形队列
        std::vector<float> presentQueuePriorities(presentQueueFamilyInfo.queueCount, 1.0f);//显示队列

        bool isSameQueueFamily = physicalDevices->isSameGraphicAndPresentQueueFamily();
        uint32_t sameQueueCount = graphicsQueueIndex;
        if(isSameQueueFamily)//队列的数量
        {
            sameQueueCount += presentQueueIndex;
            if(presentQueueIndex >= presentQueueFamilyInfo.queueCount)
            {
                spdlog::error("VulkanDevice::DeviceInit 参数错误，presentQueueIndex 超出范围");
                return;
            }
            graphicsQueuePriorities.insert(graphicsQueuePriorities.end(),presentQueuePriorities.begin(),presentQueuePriorities.end());//合并优先级列表
        }


        //创建逻辑设备 附值同一个队列族
        VkDeviceQueueCreateInfo queueCreateInfos[2]{};//最多两个队列族 
        queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[0].pNext = nullptr;//保留参数，必须为nullptr
        queueCreateInfos[0].queueFamilyIndex = static_cast<uint32_t>(graphicsQueueFamilyInfo.queueFamilyIndex);//图形队列族索引
        queueCreateInfos[0].queueCount = sameQueueCount;//创建一个队列
        queueCreateInfos[0].pQueuePriorities = graphicsQueuePriorities.data();//队列优先级列表

        if(!isSameQueueFamily)//假如不是同一个队列 就单独给显示队列附值
        {
            queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfos[1].pNext = nullptr;//保留参数，必须为nullptr
            queueCreateInfos[1].queueFamilyIndex = static_cast<uint32_t>(presentQueueFamilyInfo.queueFamilyIndex);//图形队列族索引
            queueCreateInfos[1].queueCount = presentQueueIndex;//创建一个队列
            queueCreateInfos[1].pQueuePriorities = presentQueuePriorities.data();//队列优先级列表
        }

        uint32_t availableExtensionCount;
        XJDebug_Log(vkEnumerateDeviceExtensionProperties(physicalDevice,"",&availableExtensionCount, nullptr));//获取可用的层数量
        VkExtensionProperties availableExtensions[availableExtensionCount];//定义一个数组存储可用的层
        XJDebug_Log(vkEnumerateDeviceExtensionProperties(physicalDevice, "",&availableExtensionCount, availableExtensions));//获取可用的扩展数量
        //检查环境支持什么设备扩展
        uint32_t enableExtensionCount = 0;
        const char* enableExtensions[ARRAY_SIZE(requestedExtensions) + 5];
        if(!checkDeviceFeature(
            "设备扩展",
            true,
            availableExtensionCount,
            availableExtensions,
            ARRAY_SIZE(requestedExtensions),
            requestedExtensions,
            &enableExtensionCount,
            enableExtensions))
        {
            spdlog::error("缺少必需的实例扩展，无法继续");
            return;
        }

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; 
        deviceCreateInfo.pNext = nullptr;//保留参数，必须为nullptr
        deviceCreateInfo.flags = 0;//预留参数，当前必须为0
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(isSameQueueFamily? 1:2);//如果是同一个队列就是1 不是同一个队列就2
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;//创建队列
        deviceCreateInfo.enabledLayerCount = 0;//暂时不启用层
        deviceCreateInfo.ppEnabledLayerNames = nullptr;//暂时不启用层
        deviceCreateInfo.enabledExtensionCount = enableExtensionCount;//暂时不启用扩展
        deviceCreateInfo.ppEnabledExtensionNames = enableExtensionCount > 0? enableExtensions : nullptr;//暂时不启用扩展
        deviceCreateInfo.pEnabledFeatures = nullptr;//使用物理设备的默认特性
        
        XJDebug_Log(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &mDevice));
        spdlog::trace("逻辑设备:{0}", (void*) mDevice);
        

        for(int i = 0; i < graphicsQueueIndex; i++)//渲染队列
        {
            VkQueue queue;
            vkGetDeviceQueue(mDevice, graphicsQueueFamilyInfo.queueFamilyIndex, i, &queue);
            mGraphicQueue.push_back(std::make_shared<VulkanQueue>(graphicsQueueFamilyInfo.queueFamilyIndex, i, queue, false));
        }

        for(int i = 0; i < presentQueueIndex; i++)//显示队列
        {
            VkQueue queue;
            vkGetDeviceQueue(mDevice, presentQueueFamilyInfo.queueFamilyIndex, i, &queue);
            mPresentQueue.push_back(std::make_shared<VulkanQueue>(presentQueueFamilyInfo.queueFamilyIndex, i, queue, true));
        }
        //创建管线缓存
        CreatePipelineCache();
        //创建默认命令池
        CreateDefaultCommandPool();
    }
    XJVulkanDevice::~XJVulkanDevice()
    {   
        vkDeviceWaitIdle(mDevice);
        mDefaultCmdPool = nullptr;
        vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
        vkDestroyDevice(mDevice, nullptr);
        
    }
    void XJVulkanDevice::CreatePipelineCache()//创建管线缓存
    {
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo{};
        pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;//结构体类型
        pipelineCacheCreateInfo.pNext = nullptr;
        pipelineCacheCreateInfo.flags = 0;
        pipelineCacheCreateInfo.initialDataSize = 0;
        pipelineCacheCreateInfo.pInitialData = nullptr;

        XJDebug_Log(vkCreatePipelineCache(mDevice, &pipelineCacheCreateInfo, nullptr, &mPipelineCache));
    }
    void XJVulkanDevice::CreateDefaultCommandPool()//创建默认命令池
    {
        mDefaultCmdPool = std::make_shared<XJVulkanCommandPool>(this, mPhysicalDevices->XJGetGraphicQueueFamilyInfo().queueFamilyIndex);
    }

    int32_t XJVulkanDevice::XJGetMemoryIndex(VkMemoryPropertyFlags memoryPropertyFlags, uint32_t memoryTypeBits) const
    {
        VkPhysicalDeviceMemoryProperties phyDevuceMemProps = mPhysicalDevices->XJGetPhysicalDeviceMemoryProperties();
        if (phyDevuceMemProps.memoryTypeCount == 0)
        {
            spdlog::error("VulkanDevice::XJGetMemoryIndex 获取物理设备内存属性失败，memoryTypeCount 为0");
            return -1;
        }

        for (uint32_t i = 0; i < phyDevuceMemProps.memoryTypeCount; i++)
        {
            if ((memoryTypeBits & (1 << i)) &&
                (phyDevuceMemProps.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)
            {
                return i;
            }
        }
        spdlog::critical( "XJGetMemoryIndex FAILED!\n" "  requiredFlags = 0x{:X}\n" "  memoryTypeBits = 0x{:X}", memoryPropertyFlags, memoryTypeBits);
        spdlog::error("VulkanDevice::XJGetMemoryIndex 未找到合适的内存类型");
        return -1;
    }

    VkCommandBuffer XJVulkanDevice::CreateAndBeginOneDefaultCommandBuffer()//开始单个命令缓冲区
    {
        VkCommandBuffer commandBuffer = mDefaultCmdPool->AllocateSingleCommandBuffer();
        XJVulkanCommandPool::BeginCommandBuffer(commandBuffer);
        return commandBuffer;
    }       
    void XJVulkanDevice::SubmitAndEndOneDefaultCommandBuffer(VkCommandBuffer& commandBuffer)//结束单个命令缓冲区
    {
        mDefaultCmdPool->EndCommandBuffer(commandBuffer);
        VulkanQueue* queue = XJGetFirstGraphicQueue();
        queue->Submit({commandBuffer});
        queue->WaitIdle();
    }
    VkResult XJVulkanDevice::CreateSimpleSampler(VkFilter filter, VkSamplerAddressMode addressMode, VkSampler *outSampler) 
    {
      
        VkSamplerCreateInfo samplerInfo = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .magFilter = filter,
                .minFilter = filter,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .addressModeU = addressMode,
                .addressModeV = addressMode,
                .addressModeW = addressMode,
                .mipLodBias = 0,
                .anisotropyEnable = VK_FALSE,
                .maxAnisotropy = 0,
                .compareEnable = VK_FALSE,
                .compareOp = VK_COMPARE_OP_NEVER,
                .minLod = 0,
                .maxLod = 1,
                .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                .unnormalizedCoordinates = VK_FALSE
        };
        return vkCreateSampler(mDevice, &samplerInfo, nullptr, outSampler);
    }
}