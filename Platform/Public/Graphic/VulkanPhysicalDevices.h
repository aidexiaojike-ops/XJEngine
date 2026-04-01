#ifndef VULKAN_PHYSICALDEVICES_H
#define VULKAN_PHYSICALDEVICES_H

#include "Edit/EditIncludes.h"
#include "Graphic/VulkanInstance.h"
#include "Graphic/VulkanSurface.h"


namespace XJ
{

    struct QueueFamilyInfo
    {
        int32_t queueFamilyIndex;
        uint32_t queueCount;
    };

    class VulkanPhysicalDevices
    {
        public:
            VulkanPhysicalDevices(VulkanInstance* instance, VulkanSurface* surface);
            ~VulkanPhysicalDevices();

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
