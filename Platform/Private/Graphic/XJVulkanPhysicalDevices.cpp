#include "Graphic/XJVulkanPhysicalDevices.h"


namespace XJ
{
    namespace
    {
        bool QuerySurfaceFormats(
            VkPhysicalDevice physicalDevice,
            VkSurfaceKHR surface,
            std::vector<VkSurfaceFormatKHR>& outFormats)
        {
            outFormats.clear();

            uint32_t formatCount = 0;
            VkResult result = VK_SUCCESS;

            do
            {
                result = vkGetPhysicalDeviceSurfaceFormatsKHR(
                    physicalDevice,
                    surface,
                    &formatCount,
                    nullptr);

                XJDebug_Log(result);

                if (result != VK_SUCCESS && result != VK_INCOMPLETE)
                {
                    spdlog::error(
                        "vkGetPhysicalDeviceSurfaceFormatsKHR count query failed: {}",
                        vk_result_string(result));
                    outFormats.clear();
                    return false;
                }

                if (formatCount == 0)
                {
                    spdlog::warn("Physical device has no surface formats.");
                    outFormats.clear();
                    return false;
                }

                outFormats.resize(formatCount);

                result = vkGetPhysicalDeviceSurfaceFormatsKHR(
                    physicalDevice,
                    surface,
                    &formatCount,
                    outFormats.data());

                XJDebug_Log(result);

                if (result != VK_SUCCESS && result != VK_INCOMPLETE)
                {
                    spdlog::error(
                        "vkGetPhysicalDeviceSurfaceFormatsKHR data query failed: {}",
                        vk_result_string(result));
                    outFormats.clear();
                    return false;
                }
            }
            while (result == VK_INCOMPLETE);

            outFormats.resize(formatCount);
            return !outFormats.empty();
        }
    }

    XJVulkanPhysicalDevices::XJVulkanPhysicalDevices(XJVulkanInstance* instance, XJVulkanSurface* surface)
    {
        if (!instance || !surface || !surface->IsValid())
        {
            spdlog::error("XJVulkanPhysicalDevices 参数错误，instance 或 surface 无效");
            throw std::runtime_error("XJVulkanPhysicalDevices failed: invalid instance or surface");
        }

        VkSurfaceKHR vkSurface = surface->XJGetSurface();

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
                throw std::runtime_error("XJVulkanPhysicalDevices failed: no physical devices");
            }
        
            physicalDevices.resize(physicalDeviceCount);
        
            result = vkEnumeratePhysicalDevices(
                instance->XJGetInstance(),
                &physicalDeviceCount,
                physicalDevices.data());
            
            if (result != VK_SUCCESS && result != VK_INCOMPLETE)
            {
                XJDebug_Log(result);
                throw std::runtime_error("XJVulkanPhysicalDevices failed: enumerate physical devices failed");
            }
        }
        while (result == VK_INCOMPLETE);

        physicalDevices.resize(physicalDeviceCount);

        spdlog::trace("{0} : 发现 {1} 个物理设备", __FUNCTION__, physicalDeviceCount);
        uint32_t maxScore = 0;
        int32_t maxScorePhyDeviceIndex = -1;

        QueueFamilyInfo selectedGraphicQueueFamilyInfo{ -1, 0 };
        QueueFamilyInfo selectedPresentQueueFamilyInfo{ -1, 0 };

        for(uint32_t i = 0; i < physicalDeviceCount; ++i)//打印物理设备信息
        {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
            VkDebugPhyPhysicalDevicesCallback(deviceProperties);

            uint32_t score = GetPhysicalDeviceScore(deviceProperties);

            std::vector<VkSurfaceFormatKHR> surfaceFormats;
            if (!QuerySurfaceFormats(physicalDevices[i], vkSurface, surfaceFormats))
            {
                spdlog::warn("物理设备 {} 无法查询有效表面格式，跳过。", i);
                continue;
            }

            for (uint32_t j = 0; j < static_cast<uint32_t>(surfaceFormats.size()); ++j)
            {
                if (surfaceFormats[j].format == VK_FORMAT_B8G8R8A8_SRGB &&
                    surfaceFormats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    score += 100;
                    spdlog::trace("  物理设备支持首选的表面格式 VK_FORMAT_B8G8R8A8_SRGB 和 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR");
                }
            
                spdlog::trace(
                    "  支持的表面格式 {}: Format = {}, ColorSpace = {}",
                    j,
                    vk_format_string(surfaceFormats[j].format),
                    vk_color_space_string(surfaceFormats[j].colorSpace));
            }
            if (score < maxScore)//只有更低分才跳过；同分设备允许后到者覆盖先到者
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
                VkResult presentResult = vkGetPhysicalDeviceSurfaceSupportKHR(
                    physicalDevices[i],
                    k,
                    vkSurface,
                    &presentSupport);
                
                if (presentResult != VK_SUCCESS)
                {
                    XJDebug_Log(presentResult);
                    spdlog::warn("查询队列族 {} 的 present support 失败: {}", k, vk_result_string(presentResult));
                    presentSupport = VK_FALSE;
                }
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
            maxScorePhyDeviceIndex = static_cast<int32_t>(i);
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
    XJVulkanPhysicalDevices::~XJVulkanPhysicalDevices()
    {
        //待实现
        spdlog::trace("{0} : 销毁 physicalDevice 物理设备 : {1}", __FUNCTION__, (void*)physicalDevice);       

    }
    void XJVulkanPhysicalDevices::VkDebugPhyPhysicalDevicesCallback(VkPhysicalDeviceProperties &deviceProperties)
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
    uint32_t XJVulkanPhysicalDevices::GetPhysicalDeviceScore(VkPhysicalDeviceProperties &deviceProperties)//设备评分
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
