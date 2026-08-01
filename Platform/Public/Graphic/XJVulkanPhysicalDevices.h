#ifndef XJ_VULKAN_PHYSICALDEVICES_H
#define XJ_VULKAN_PHYSICALDEVICES_H

#include "Edit/EditIncludes.h"
#include "Graphic/XJVulkanInstance.h"
#include "Graphic/XJVulkanSurface.h"


namespace XJ
{

    struct QueueFamilyInfo
    {
        int32_t queueFamilyIndex = -1;
        uint32_t queueCount = 0;
    };

    class XJVulkanPhysicalDevices
    {
        public:
            XJVulkanPhysicalDevices(XJVulkanInstance* instance, XJVulkanSurface* surface);
            ~XJVulkanPhysicalDevices();

            VkPhysicalDevice XJGetPhysicalDevice() { return physicalDevice; }
            VkPhysicalDeviceMemoryProperties XJGetPhysicalDeviceMemoryProperties() { return memoryProperties; }

            const QueueFamilyInfo& XJGetGraphicQueueFamilyInfo() const { return GraphicQueueFamilyInfo; }
            const QueueFamilyInfo& XJGetPresentQueueFamilyInfo() const { return PresentQueueFamilyInfo; }

            bool isSameGraphicAndPresentQueueFamily() const { return GraphicQueueFamilyInfo.queueFamilyIndex == PresentQueueFamilyInfo.queueFamilyIndex; }
        private:
            static void VkDebugPhyPhysicalDevicesCallback(VkPhysicalDeviceProperties &deviceProperties);
            static uint32_t GetPhysicalDeviceScore(VkPhysicalDeviceProperties &deviceProperties);

            VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
            VkPhysicalDeviceMemoryProperties memoryProperties;


            QueueFamilyInfo GraphicQueueFamilyInfo;//渲染管线
            QueueFamilyInfo PresentQueueFamilyInfo;//显示管线


    };
}


#endif
