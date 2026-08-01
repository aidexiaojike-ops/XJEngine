#ifndef XJ_VULKAN_INSTANCE_H
#define XJ_VULKAN_INSTANCE_H

#include "Graphic/VulkanCommon.h"
#include "Edit/EditIncludes.h"
#include "Edit/SpdlogDebug.h"



namespace XJ
{
    class XJVulkanInstance
    {
        private:
            /* data */
            uint32_t availableLayerCount = 0;
            uint32_t availableExtensionCount = 0;
            uint32_t mApiVersion = VK_API_VERSION_1_0;
        public:
            XJVulkanInstance();
            ~XJVulkanInstance();

            XJVulkanInstance(const XJVulkanInstance&) = delete;
            XJVulkanInstance& operator=(const XJVulkanInstance&) = delete;
            
            VkInstance mInstance = nullptr;
            bool bShouldValidate = true;

            VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

            VkInstance XJGetInstance() const { return mInstance; }
            uint32_t XJGetApiVersion() const { return mApiVersion; }
    };
    
}

#endif
