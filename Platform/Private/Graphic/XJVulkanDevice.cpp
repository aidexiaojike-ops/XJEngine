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
    XJVulkanDevice::XJVulkanDevice(VulkanPhysicalDevices* physicalDevices, uint32_t graphicsQueueCount, uint32_t presentQueueCount, const VkSettings &settings) 
    :settings(settings), mPhysicalDevices(physicalDevices)
    {
        if(!physicalDevices)
        {
            spdlog::error("XJVulkanDevice::XJVulkanDevice 参数错误，physicalDevices 为空指针");
            throw std::runtime_error("XJVulkanDevice failed: physicalDevices is null");
        }
        QueueFamilyInfo graphicsQueueFamilyInfo = physicalDevices->XJGetGraphicQueueFamilyInfo();
        QueueFamilyInfo presentQueueFamilyInfo = physicalDevices->XJGetPresentQueueFamilyInfo();
        VkPhysicalDevice physicalDevice = physicalDevices->XJGetPhysicalDevice();
        //配置队列创建信息
        if (graphicsQueueCount == 0)
        {
            spdlog::error("VulkanDevice::DeviceInit 参数错误，graphicsQueueCount 不能为 0");
            throw std::runtime_error("XJVulkanDevice failed: graphicsQueueCount is zero");
        }

        if (presentQueueCount == 0)
        {
            spdlog::error("VulkanDevice::DeviceInit 参数错误，presentQueueCount 不能为 0");
            throw std::runtime_error("XJVulkanDevice failed: presentQueueCount is zero");
        }

        bool isSameQueueFamily = physicalDevices->isSameGraphicAndPresentQueueFamily();

        uint32_t requestedGraphicsQueueCount = graphicsQueueCount;
        uint32_t requestedPresentQueueCount = presentQueueCount;
        uint32_t createdGraphicsQueueCount = 0;
        uint32_t createdPresentQueueCount = 0;
        uint32_t presentQueueStartIndex = 0;

        std::vector<float> graphicsQueuePriorities;
        std::vector<float> presentQueuePriorities;

        //创建逻辑设备 附值同一个队列族
        VkDeviceQueueCreateInfo queueCreateInfos[2]{};//最多两个队列族 
        uint32_t queueCreateInfoCount = 0;

        if(isSameQueueFamily)//队列的数量
        {
            uint32_t requiredQueueCount = requestedGraphicsQueueCount + requestedPresentQueueCount;
            if (requiredQueueCount > graphicsQueueFamilyInfo.queueCount)
            {
                spdlog::warn(
                    "同一队列族可用队列不足，Graphics={}, Present={}, Available={}。Present 将复用 Graphics 队列。",
                    requestedGraphicsQueueCount,
                    requestedPresentQueueCount,
                    graphicsQueueFamilyInfo.queueCount);
                
                requiredQueueCount = std::max(requestedGraphicsQueueCount, requestedPresentQueueCount);
                presentQueueStartIndex = 0;
            }
            else
            {
                presentQueueStartIndex = requestedGraphicsQueueCount;
            }
        
            if (requiredQueueCount == 0 || requiredQueueCount > graphicsQueueFamilyInfo.queueCount)
            {
                spdlog::error(
                    "VulkanDevice::DeviceInit 参数错误，同队列族请求队列数无效，required={}, available={}",
                    requiredQueueCount,
                    graphicsQueueFamilyInfo.queueCount);
                throw std::runtime_error("XJVulkanDevice failed: invalid same-family queue count");
            }
        
            graphicsQueuePriorities.resize(requiredQueueCount, 0.0f);
        
            for (uint32_t i = presentQueueStartIndex; i < presentQueueStartIndex + requestedPresentQueueCount && i < requiredQueueCount; ++i)
            {
                graphicsQueuePriorities[i] = 1.0f;
            }
        
            queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfos[0].pNext = nullptr;
            queueCreateInfos[0].queueFamilyIndex = static_cast<uint32_t>(graphicsQueueFamilyInfo.queueFamilyIndex);;//图形队列族索引
            queueCreateInfos[0].queueCount = requiredQueueCount;//创建一个队列
            queueCreateInfos[0].pQueuePriorities = graphicsQueuePriorities.data();//队列优先级列表
        
            queueCreateInfoCount = 1;
            createdGraphicsQueueCount = requestedGraphicsQueueCount;
            createdPresentQueueCount = requestedPresentQueueCount;
        }
        else
        {
            if (requestedGraphicsQueueCount > graphicsQueueFamilyInfo.queueCount)
            {
                spdlog::error(
                    "VulkanDevice::DeviceInit 参数错误，graphicsQueueCount 超出范围，requested={}, available={}",
                    requestedGraphicsQueueCount,
                    graphicsQueueFamilyInfo.queueCount);
                return;
            }
        
            if (requestedPresentQueueCount > presentQueueFamilyInfo.queueCount)
            {
                spdlog::error(
                    "VulkanDevice::DeviceInit 参数错误，presentQueueCount 超出范围，requested={}, available={}",
                    requestedPresentQueueCount,
                    presentQueueFamilyInfo.queueCount);
                throw std::runtime_error("XJVulkanDevice failed: graphicsQueueCount exceeds available queue count");
            }
        
            graphicsQueuePriorities.resize(requestedGraphicsQueueCount, 0.0f);
            presentQueuePriorities.resize(requestedPresentQueueCount, 1.0f);
        
            queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfos[0].pNext = nullptr;
            queueCreateInfos[0].queueFamilyIndex = static_cast<uint32_t>(graphicsQueueFamilyInfo.queueFamilyIndex);
            queueCreateInfos[0].queueCount = requestedGraphicsQueueCount;
            queueCreateInfos[0].pQueuePriorities = graphicsQueuePriorities.data();
        
            queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfos[1].pNext = nullptr;
            queueCreateInfos[1].queueFamilyIndex = static_cast<uint32_t>(presentQueueFamilyInfo.queueFamilyIndex);
            queueCreateInfos[1].queueCount = requestedPresentQueueCount;
            queueCreateInfos[1].pQueuePriorities = presentQueuePriorities.data();
        
            queueCreateInfoCount = 2;
            createdGraphicsQueueCount = requestedGraphicsQueueCount;
            createdPresentQueueCount = requestedPresentQueueCount;
        }

        uint32_t availableExtensionCount;
        XJDebug_Log(vkEnumerateDeviceExtensionProperties(physicalDevice,"",&availableExtensionCount, nullptr));//获取可用的层数量
        std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);//定义一个数组存储可用的层
        if (availableExtensionCount > 0)
        {
            XJDebug_Log(vkEnumerateDeviceExtensionProperties(
                physicalDevice,
                "",
                &availableExtensionCount,
                availableExtensions.data()));
        }//获取可用的扩展数量
        //检查环境支持什么设备扩展
        uint32_t enableExtensionCount = 0;
        const char* enableExtensions[ARRAY_SIZE(requestedExtensions) + 5];
        if(!checkDeviceFeature(
            "设备扩展",
            true,
            availableExtensionCount,
            availableExtensions.data(),
            ARRAY_SIZE(requestedExtensions),
            requestedExtensions,
            &enableExtensionCount,
            enableExtensions))
        {
            spdlog::error("缺少必需的实例扩展，无法继续");
            throw std::runtime_error("XJVulkanDevice failed: missing required device extensions");
        }

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; 
        deviceCreateInfo.pNext = nullptr;//保留参数，必须为nullptr
        deviceCreateInfo.flags = 0;//预留参数，当前必须为0
        deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;//如果是同一个队列就是1 不是同一个队列就2
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;//创建队列
        deviceCreateInfo.enabledLayerCount = 0;//暂时不启用层
        deviceCreateInfo.ppEnabledLayerNames = nullptr;//暂时不启用层
        deviceCreateInfo.enabledExtensionCount = enableExtensionCount;//暂时不启用扩展
        deviceCreateInfo.ppEnabledExtensionNames = enableExtensionCount > 0? enableExtensions : nullptr;//暂时不启用扩展
        deviceCreateInfo.pEnabledFeatures = nullptr;//使用物理设备的默认特性
        
        VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &mDevice);
        XJDebug_Log(result);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("XJVulkanDevice failed: vkCreateDevice failed");
        }
        spdlog::trace("逻辑设备:{0}", (void*) mDevice);
        

        for(uint32_t i = 0; i < createdGraphicsQueueCount; i++)//渲染队列
        {
            VkQueue queue = VK_NULL_HANDLE;
            vkGetDeviceQueue(
                mDevice,
                static_cast<uint32_t>(graphicsQueueFamilyInfo.queueFamilyIndex),
                i,
                &queue);
            
            mGraphicQueue.push_back(std::make_shared<VulkanQueue>(
                static_cast<uint32_t>(graphicsQueueFamilyInfo.queueFamilyIndex),
                i,
                queue,
                false));
        }

        for(uint32_t i = 0; i < createdPresentQueueCount; i++)//显示队列
        {
            uint32_t queueIndex = isSameQueueFamily ? presentQueueStartIndex + i : i;
        
            VkQueue queue = VK_NULL_HANDLE;
            vkGetDeviceQueue(
                mDevice,
                static_cast<uint32_t>(presentQueueFamilyInfo.queueFamilyIndex),
                queueIndex,
                &queue);
            
            mPresentQueue.push_back(std::make_shared<VulkanQueue>(
                static_cast<uint32_t>(presentQueueFamilyInfo.queueFamilyIndex),
                queueIndex,
                queue,
                true));
        }
        //创建管线缓存
        CreatePipelineCache();
        //创建默认命令池
        CreateDefaultCommandPool();
    }
    XJVulkanDevice::~XJVulkanDevice()
    {   
        
        if (mDevice == VK_NULL_HANDLE)
        {
            return;
        }

        vkDeviceWaitIdle(mDevice);

        mDefaultCmdPool = nullptr;

        if (mPipelineCache != VK_NULL_HANDLE)
        {
            vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
            mPipelineCache = VK_NULL_HANDLE;
        }

        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
        
    }
    void XJVulkanDevice::CreatePipelineCache()//创建管线缓存
    {
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo{};
        pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;//结构体类型
        pipelineCacheCreateInfo.pNext = nullptr;
        pipelineCacheCreateInfo.flags = 0;
        pipelineCacheCreateInfo.initialDataSize = 0;
        pipelineCacheCreateInfo.pInitialData = nullptr;

        VkResult result = vkCreatePipelineCache(mDevice, &pipelineCacheCreateInfo, nullptr, &mPipelineCache);
        XJDebug_Log(result);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("XJVulkanDevice failed: vkCreatePipelineCache failed");
        }
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

        mDefaultCmdPool->FreeCommandBuffer(commandBuffer);
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