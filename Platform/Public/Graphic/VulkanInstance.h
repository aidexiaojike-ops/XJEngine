#ifndef VULKAN_INSTANCE_H
#define VULKAN_INSTANCE_H

#include "Graphic/VulkanCommon.h"
#include "Edit/EditIncludes.h"
#include "Edit/SpdlogDebug.h"



namespace XJ
{
    class VulkanInstance
    {
        private:
            /* data */
            uint32_t availableLayerCount;
            uint32_t availableExtensionCount;
        public:
            VulkanInstance();
            ~VulkanInstance();
            VkInstance mInstance = nullptr;
            bool bShouldValidate = true;

            VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

            VkInstance XJGetInstance() { return mInstance; }
    };
    
}

#endif