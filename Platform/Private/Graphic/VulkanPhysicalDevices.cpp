#include "graphic/VulkanPhysicalDevices.h"


namespace XJ
{
    VulkanPhysicalDevices::VulkanPhysicalDevices(VulkanInstance* instance, VulkanSurface* surface)
    {
        //查询所有的物理设备
        uint32_t physicalDeviceCount = 0;
        std::vector<VkPhysicalDevice> physicalDevices;
        VkResult result = VK_SUCCESS;

        do
        {
            XJDebug_Log(vkEnumeratePhysicalDevices(instance->XJGetInstance(), &physicalDeviceCount, nullptr));
        
            if (physicalDeviceCount == 0)
            {
                spdlog::error("未发现 Vulkan 物理设备");
                throw std::runtime_error("VulkanPhysicalDevices failed: no physical devices");
            }
        
            physicalDevices.resize(physicalDeviceCount);
        
            result = vkEnumeratePhysicalDevices(
                instance->XJGetInstance(),
                &physicalDeviceCount,
                physicalDevices.data());
            
            if (result != VK_SUCCESS && result != VK_INCOMPLETE)
            {
                XJDebug_Log(result);
                throw std::runtime_error("VulkanPhysicalDevices failed: enumerate physical devices failed");
            }
        }
        while (result == VK_INCOMPLETE);

        physicalDevices.resize(physicalDeviceCount);

        spdlog::trace("{0} : 发现 {1} 个物理设备", __FUNCTION__, physicalDeviceCount);
        uint32_t maxScore = 0;
        int32_t maxScorePhyDeviceIndex = -1;

        QueueFamilyInfo selectedGraphicQueueFamilyInfo{ -1, 0 };
        QueueFamilyInfo selectedPresentQueueFamilyInfo{ -1, 0 };

        for(int32_t i = 0; i < static_cast<int32_t>(physicalDeviceCount); i++)//打印物理设备信息
        {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
            VkDebugPhyPhysicalDevicesCallback(deviceProperties);

            uint32_t score = GetPhysicalDeviceScore(deviceProperties);

            uint32_t formatCount = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevices[i], surface->mSurface, &formatCount, nullptr);

            std::vector<VkSurfaceFormatKHR> surfaceFormats;
            VkResult surfaceResult = VK_SUCCESS;

            do
            {
                XJDebug_Log(vkGetPhysicalDeviceSurfaceFormatsKHR(
                    physicalDevices[i],
                    surface->mSurface,
                    &formatCount,
                    nullptr));
                
                surfaceFormats.resize(formatCount);
                
                surfaceResult = formatCount > 0
                    ? vkGetPhysicalDeviceSurfaceFormatsKHR(
                        physicalDevices[i],
                        surface->mSurface,
                        &formatCount,
                        surfaceFormats.data())
                    : VK_SUCCESS;
                    
                if (surfaceResult != VK_SUCCESS && surfaceResult != VK_INCOMPLETE)
                {
                    XJDebug_Log(surfaceResult);
                    surfaceFormats.clear();
                    break;
                }
            }
            while (surfaceResult == VK_INCOMPLETE);

            surfaceFormats.resize(formatCount);
          
            for(uint32_t j = 0; j < formatCount; j++)
            {
                if(surfaceFormats[j].format == VK_FORMAT_B8G8R8A8_SRGB &&
                   surfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    score += 100;
                    spdlog::trace("  物理设备支持首选的表面格式 VK_FORMAT_B8G8R8A8_SRGB 和 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR");
                }
                spdlog::trace("  支持的表面格式 {}: Format = {}, ColorSpace = {}", j, vk_format_string(surfaceFormats[j].format), vk_color_space_string(surfaceFormats[j].colorSpace));
            }
            if(score < maxScore)//当前设备分数不可能成为更优选择
            {
                continue;
            } 
            
            // ★ 必须：初始化为无效值
            QueueFamilyInfo localGraphicQueueFamilyInfo{ -1, 0 };
            QueueFamilyInfo localPresentQueueFamilyInfo{ -1, 0 };
        
            uint32_t queueFamilyCount = 0;
            //查询队列族数量
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyCount, nullptr);
            //VkQueueFamilyProperties queueFamilies[queueFamilyCount];
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            //查询队列族属性
            if (queueFamilyCount > 0)
            {
                vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyCount, queueFamilies.data());
            }
            
            spdlog::trace("---------------------检查队列--------------------");
            
            for(uint32_t  k = 0; k < queueFamilyCount; k++)
            {
                if(queueFamilies[k].queueCount == 0)
                {
                    spdlog::warn("队列族 {} 没有可用的队列，跳过该队列族的检查。", k);
                    continue;
                }
                //检查图形队列支持
                if(localGraphicQueueFamilyInfo.queueFamilyIndex == -1 &&
                   (queueFamilies[k].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                {
                    localGraphicQueueFamilyInfo.queueFamilyIndex = static_cast<int32_t>(k);
                    localGraphicQueueFamilyInfo.queueCount = queueFamilies[k].queueCount;
                    spdlog::trace("队列族 {} 支持图形操作。", k);
                }
            
                //检查显示队列支持
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[i], k, /*VkSurfaceKHR*/surface->mSurface, &presentSupport);
                if(localPresentQueueFamilyInfo.queueFamilyIndex == -1 && presentSupport)
                {
                    localPresentQueueFamilyInfo.queueFamilyIndex = static_cast<int32_t>(k);
                    localPresentQueueFamilyInfo.queueCount = queueFamilies[k].queueCount;
                    spdlog::trace("队列族 {} 支持显示操作。", k);
                }
            
                //打印队列族信息
                if(localGraphicQueueFamilyInfo.queueFamilyIndex != -1 &&
                   localPresentQueueFamilyInfo.queueFamilyIndex != -1)
                {
                    spdlog::trace(
                        "已找到图形和显示队列族，索引分别为 {} 和 {}。",
                        localGraphicQueueFamilyInfo.queueFamilyIndex,
                        localPresentQueueFamilyInfo.queueFamilyIndex);
                }

                spdlog::trace("队列族 {}: 队列标志 = {}, 队列数量 = {}, 时间戳有效位 = {}, 最小图像传输粒度 = ({}, {}, {})",
                    k,
                    queueFamilies[k].queueFlags,
                    queueFamilies[k].queueCount,
                    queueFamilies[k].timestampValidBits,
                    queueFamilies[k].minImageTransferGranularity.width,
                    queueFamilies[k].minImageTransferGranularity.height,
                    queueFamilies[k].minImageTransferGranularity.depth
                );
                
            }
            
            spdlog::trace("---------------------检查队列完成--------------------");
            bool hasRequiredQueueFamilies =
                localGraphicQueueFamilyInfo.queueFamilyIndex != -1 &&
                localPresentQueueFamilyInfo.queueFamilyIndex != -1;

            if (!hasRequiredQueueFamilies)
            {
                spdlog::warn("物理设备 {} 缺少有效的图形或显示队列族，跳过。", i);
                continue;
            }
        
            maxScore = score;
            maxScorePhyDeviceIndex = i;
            selectedGraphicQueueFamilyInfo = localGraphicQueueFamilyInfo;
            selectedPresentQueueFamilyInfo = localPresentQueueFamilyInfo;
        
            spdlog::trace(
                "当前最佳物理设备更新为 {}，score={}, Graphics={}, Present={}",
                i,
                score,
                selectedGraphicQueueFamilyInfo.queueFamilyIndex,
                selectedPresentQueueFamilyInfo.queueFamilyIndex);
        }

        if(maxScorePhyDeviceIndex < 0)
        {
            spdlog::error("未找到合适的物理设备，无法继续初始化 Vulkan 物理设备。");
            return;
        }
        physicalDevice = physicalDevices[maxScorePhyDeviceIndex];
        GraphicQueueFamilyInfo = selectedGraphicQueueFamilyInfo;
        PresentQueueFamilyInfo = selectedPresentQueueFamilyInfo;

        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);//申请buffer时需要   
        spdlog::trace(
            "{0} : 选择物理设备索引 {1} 作为主要设备，Graphics={2}, Present={3}",
            __FUNCTION__,
            maxScorePhyDeviceIndex,
            GraphicQueueFamilyInfo.queueFamilyIndex,
            PresentQueueFamilyInfo.queueFamilyIndex);
    }
    VulkanPhysicalDevices::~VulkanPhysicalDevices()
    {
        //待实现
        spdlog::trace("{0} : 销毁 physicalDevice 物理设备 : {1}", __FUNCTION__, (void*)physicalDevice);       

    }
    void VulkanPhysicalDevices::VkDebugPhyPhysicalDevicesCallback(VkPhysicalDeviceProperties &deviceProperties)
    {
        spdlog::trace("---------------------检查物理设备--------------------");

        const char* deviceTypeStr = 
            deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "集成GPU" :
            deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "独立GPU" :
            deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? "虚拟GPU" :
            deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? "CPU" :
            "其他类型GPU";

        spdlog::trace("物理设备名称: {}", deviceProperties.deviceName);
        spdlog::trace("物理设备类型: {}", deviceTypeStr);
        spdlog::trace("物理设备ID: {}", deviceProperties.deviceID);
        spdlog::trace("物理设备供应商ID: {}", deviceProperties.vendorID);
        spdlog::trace("物理设备API版本: {}.{}.{}", 
            VK_VERSION_MAJOR(deviceProperties.apiVersion),
            VK_VERSION_MINOR(deviceProperties.apiVersion),
            VK_VERSION_PATCH(deviceProperties.apiVersion)
        );
        spdlog::trace("显卡驱动版本（厂商自定义）: {}.{}.{}", 
            VK_VERSION_MAJOR(deviceProperties.driverVersion),
            VK_VERSION_MINOR(deviceProperties.driverVersion),
            VK_VERSION_PATCH(deviceProperties.driverVersion)
        );

         spdlog::trace("---------------------物理设备完成--------------------");
    }
    uint32_t VulkanPhysicalDevices::GetPhysicalDeviceScore(VkPhysicalDeviceProperties &deviceProperties)//设备评分
    {
        VkPhysicalDeviceType deviceType = deviceProperties.deviceType;
        uint32_t score = 0;
        switch (deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                score += 1000;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                score += 500;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                score += 300;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                score += 100;
                break;
            default:
                score += 10;
                break;
        }
        return score;
    }
}