#define VK_USE_PLATFORM_WIN32_KHR
#include "Graphic/VulkanInstance.h"
#include "Graphic/VulkanCommon.h"
#include "Edit/XJGlfwWindow.h"
#include <unordered_set>

// 确保调试扩展常量被正确定义（兜底）
#ifndef VK_EXT_debug_report_EXTENSION_NAME
#define VK_EXT_debug_report_EXTENSION_NAME "VK_EXT_debug_report"
#endif

namespace XJ
{
    const DeviceFeature requiredLayers[] = {
        {"VK_LAYER_KHRONOS_validation", true},// 必需启用标准验证层
    };
    const DeviceFeature requiredExtensions[] = {
        { VK_KHR_SURFACE_EXTENSION_NAME, true },    // 所有平台都需要此扩展
        { VK_KHR_WIN32_SURFACE_EXTENSION_NAME, true },  //Windows 平台需要此扩展
        { VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true },   
        // 可以根据需要添加其他可选的实例层
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
    {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            spdlog::error("[Vulkan] {}", pCallbackData->pMessage);
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            spdlog::warn("[Vulkan] {}", pCallbackData->pMessage);
        }
        else
        {
            spdlog::info("[Vulkan] {}", pCallbackData->pMessage);
        }

        return VK_FALSE;
    }

    VulkanInstance::VulkanInstance()
    {
       //构建支持的层 获取当前系统（显卡驱动 + 操作系统）支持的所有「Vulkan 实例层」的列表和详细信息。
        XJDebug_Log(vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr));//获取可用的层数量
        VkLayerProperties availableLayers[availableLayerCount];//定义一个数组存储可用的层
        XJDebug_Log(vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers));//获取可用的层信息

        uint32_t enableLayerCount = 0;
        const char* enableLayers[ARRAY_SIZE(requiredLayers)];
        if(bShouldValidate)
        {
            if(!checkDeviceFeature(
            "实例层",
            false,
            availableLayerCount,
            availableLayers,
            ARRAY_SIZE(requiredLayers),
            requiredLayers,
            &enableLayerCount,
            enableLayers))
            {
                spdlog::error("缺少必需的实例层，无法继续。");
                return;
            }

        }
      
        spdlog::trace("所有必需的实例层均已找到。");
    

        //构建支持的扩展 获取当前系统（显卡驱动 + 操作系统）支持的所有「Vulkan 实例扩展」的列表
        XJDebug_Log(vkEnumerateInstanceExtensionProperties("",&availableExtensionCount, nullptr));//获取可用的层数量
        VkExtensionProperties availableExtensions[availableExtensionCount];//定义一个数组存储可用的层
        XJDebug_Log(vkEnumerateInstanceExtensionProperties("",&availableExtensionCount, availableExtensions));//获取可用的扩展数量
        //收集 Vulkan 实例需要启用的扩展列表，并对扩展进行「去重」，确保每个扩展只被添加一次
        uint32_t glfwRequestedExtensionCount = 0;
        const char** glfwRequestedExtensions = glfwGetRequiredInstanceExtensions(&glfwRequestedExtensionCount);//返回 GLFW 运行 Vulkan 必需的扩展列表
        std::unordered_set<const char*> allRequestedExtensionSet;//查重的容器
        std::vector<DeviceFeature> allRequestedExtensions;//业务层计划请求的扩展列表
        for(const auto &item: requiredExtensions)//先添加业务层请求的扩展
        {
            if(allRequestedExtensionSet.find(item.name) == allRequestedExtensionSet.end())//判断每个扩展是否已经存在于查重集合
            {
                allRequestedExtensionSet.insert(item.name);
                allRequestedExtensions.push_back(item);
            }
             
        }
        for(int i = 0; i < glfwRequestedExtensionCount; i++)
        {
            const char* extensionName = glfwRequestedExtensions[i];// 核心判断：GLFW要求的这个扩展是否未被加入过（去重）
            if(allRequestedExtensionSet.find(glfwRequestedExtensions[i]) == allRequestedExtensionSet.end())
            {
                allRequestedExtensionSet.insert(extensionName); // 未存在 → 加入查重集合（标记为已添加）
                allRequestedExtensions.push_back({extensionName, true}); // 加入最终扩展列表，标记为「启用」（true）
            }
        }

       
        uint32_t enableExtensionCount = 0;
        const char* enableExtensions[ARRAY_SIZE(requiredExtensions) + 5];
        if(!checkDeviceFeature(
            "实例扩展",
            true,
            availableExtensionCount,
            availableExtensions,
            allRequestedExtensions.size(),
            allRequestedExtensions.data(),
            &enableExtensionCount,
            enableExtensions))
        {
            spdlog::error("缺少必需的实例扩展，无法继续");
            return;
        }
        spdlog::trace("所有必需的实例扩展均已找到。");
        //创建Vulkan实例
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext = nullptr;
        appInfo.pApplicationName = "XJEngine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No XJEngine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;
        //去debug createInfo
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.pNext = nullptr;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = DebugUtilsCallback;
        debugCreateInfo.pUserData = nullptr;
        if (bShouldValidate)
        {
            auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT");
        
            if (func != nullptr)
            {
                func(mInstance, &debugCreateInfo, nullptr, &debugMessenger);
                spdlog::trace("Debug Utils Messenger 创建成功");
            }
            else
            {
                spdlog::error("vkCreateDebugUtilsMessengerEXT not found");
            }
        }
       
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pNext = bShouldValidate ? &debugCreateInfo : nullptr;
        createInfo.flags = 0;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = enableLayerCount;
        createInfo.ppEnabledLayerNames = enableLayerCount > 0 ? enableLayers : nullptr;
        createInfo.enabledExtensionCount = enableExtensionCount;
        createInfo.ppEnabledExtensionNames = enableExtensionCount > 0 ? enableExtensions : nullptr;

        XJDebug_Log(vkCreateInstance(&createInfo, nullptr, &mInstance));
        spdlog::trace("{0} : 创建 instance 实例 : {1}", __FUNCTION__, (void*)mInstance);
    }

    VulkanInstance::~VulkanInstance()
    {
        
        if (debugMessenger != VK_NULL_HANDLE)
        {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT");
        
            if (func != nullptr)
            {
                func(mInstance, debugMessenger, nullptr);
            }
        }
        if (mInstance != nullptr)
        {
            vkDestroyInstance(mInstance, nullptr);
            spdlog::trace("{0} : 销毁 instance 实例 : {1}", __FUNCTION__, (void*)mInstance);
            mInstance = nullptr;
        }
    }

}