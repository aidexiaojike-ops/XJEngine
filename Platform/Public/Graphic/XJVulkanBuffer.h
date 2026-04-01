#ifndef XJ_VULKAN_BUFFER_H
#define XJ_VULKAN_BUFFER_H

#include "Graphic/VulkanCommon.h"

namespace XJ
{
    class XJVulkanDevice;

    class XJVulkanBuffer
    {
        private:
            /* data */
            XJVulkanDevice* mDevice = nullptr;
            size_t mSize = 0;
            
            VkBuffer mBuffer = VK_NULL_HANDLE;
            VkDeviceMemory mBufferMemory = VK_NULL_HANDLE;

            void CreateBuffer(VkBufferUsageFlags usage, const void* data);
            bool bHostVisible;
        public:
            XJVulkanBuffer(XJVulkanDevice* device, VkBufferUsageFlags usage, size_t size,const void* data = nullptr,bool bHostVisible = false);
            ~XJVulkanBuffer();
            
            static void CreateBufferInternal(XJVulkanDevice* device, VkMemoryPropertyFlags memoryPropertyFlags, VkBufferUsageFlags usage, size_t size, VkBuffer* outBuffer, VkDeviceMemory* outBufferMemory);
            static void CopyToBuffer(XJVulkanDevice* device, VkBuffer srcBuffer = VK_NULL_HANDLE, VkBuffer dstBuffer = VK_NULL_HANDLE, size_t size = 0);
            
            VkResult WriteData(void *data);

            VkBuffer XJGetBuffer() const { return mBuffer; }
        };
    }


#endif 