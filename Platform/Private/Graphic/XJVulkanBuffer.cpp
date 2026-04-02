#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/VulkanQueue.h"
#include "Graphic/XJVulkanCommandBuffer.h"

namespace XJ
{
    
    XJVulkanBuffer::XJVulkanBuffer(XJVulkanDevice* device, VkBufferUsageFlags usage, size_t size,const void* data, bool bHostVisible)
                : mDevice(device), mSize(size), bHostVisible(bHostVisible)
    {
       CreateBuffer(usage, data);
    }
    XJVulkanBuffer::~XJVulkanBuffer()
    {
        vkDestroyBuffer(mDevice->XJGetDevice(), mBuffer, nullptr);
        vkFreeMemory(mDevice->XJGetDevice(), mBufferMemory, nullptr);
    }
    void XJVulkanBuffer::CreateBufferInternal(XJVulkanDevice* device, VkMemoryPropertyFlags memoryPropertyFlags, VkBufferUsageFlags usage, size_t size, VkBuffer* outBuffer, VkDeviceMemory* outBufferMemory)
    {
        VkBufferCreateInfo bufferInfo{};//缓冲区创建信息
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.pNext = nullptr;
        bufferInfo.flags = 0;
        bufferInfo.size = size; //缓冲区大小
        bufferInfo.usage = usage;//缓冲区使用标志
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.queueFamilyIndexCount = 0;
        bufferInfo.pQueueFamilyIndices = nullptr;

        XJDebug_Log(vkCreateBuffer(device->XJGetDevice(), &bufferInfo, nullptr, outBuffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device->XJGetDevice(), *outBuffer, &memRequirements);
        //分配内存
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = static_cast<uint32_t>(device->XJGetMemoryIndex(memoryPropertyFlags, memRequirements.memoryTypeBits));
        //allocInfo.memoryTypeIndex = static_cast<uint32_t>(device->XJGetMemoryIndex(memoryPropertyFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    
        XJDebug_Log(vkAllocateMemory(device->XJGetDevice(), &allocInfo, nullptr, outBufferMemory));
        XJDebug_Log(vkBindBufferMemory(device->XJGetDevice(), *outBuffer, *outBufferMemory, 0));
    }

    void XJVulkanBuffer::CopyToBuffer(XJVulkanDevice* device, VkBuffer srcBuffer, VkBuffer dstBuffer, size_t size)
    {
        VkCommandBuffer commandBuffer = device->CreateAndBeginOneDefaultCommandBuffer();

        VkBufferCopy bufferCopy{};
        bufferCopy.srcOffset = 0;
        bufferCopy.dstOffset = 0;
        bufferCopy.size = size;
        
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &bufferCopy);
        device->SubmitAndEndOneDefaultCommandBuffer(commandBuffer);

    }
    void XJVulkanBuffer::CreateBuffer(VkBufferUsageFlags usage, const void* data)
    {
        if(bHostVisible)
        {
             //创建临时缓冲区
            CreateBufferInternal(mDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, mSize, &mBuffer, &mBufferMemory);
    
            //将数据复制到临时缓冲区
            if(data)
            {
                void *mapping;
                XJDebug_Log(vkMapMemory(mDevice->XJGetDevice(), mBufferMemory, 0, mSize, 0, &mapping));
                memcpy(mapping, data, mSize);
                vkUnmapMemory(mDevice->XJGetDevice(), mBufferMemory);
            }
        }
        else
        {
            //创建临时缓冲区用于数据传输
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
             //创建临时缓冲区
            CreateBufferInternal(mDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, mSize, &stagingBuffer, &stagingBufferMemory);
    
            //将数据复制到临时缓冲区
            if(data)
            {
                void *mapping;
                XJDebug_Log(vkMapMemory(mDevice->XJGetDevice(), stagingBufferMemory, 0, mSize, 0, &mapping));
                memcpy(mapping, data, mSize);
                vkUnmapMemory(mDevice->XJGetDevice(), stagingBufferMemory);

            }
            //创建实际使用的缓冲区
            CreateBufferInternal(mDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 usage|VK_BUFFER_USAGE_TRANSFER_DST_BIT, mSize, &mBuffer, &mBufferMemory);
    
            //将数据从临时缓冲区复制到实际使用的缓冲区
            CopyToBuffer(mDevice, stagingBuffer, mBuffer, mSize);
    
            vkDestroyBuffer(mDevice->XJGetDevice(), stagingBuffer, nullptr);
            vkFreeMemory(mDevice->XJGetDevice(), stagingBufferMemory, nullptr);
        }

    }
    VkResult XJVulkanBuffer::WriteData(void *data)
    {
        if(data && bHostVisible)
        {
            void *mapping;
            VkResult ret = vkMapMemory(mDevice->XJGetDevice(), mBufferMemory, 0, mSize, 0, &mapping);
            XJDebug_Log(ret);
            memcpy(mapping, data, mSize);
            vkUnmapMemory(mDevice->XJGetDevice(), mBufferMemory);
            return ret;
        }
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult XJVulkanBuffer::WriteDataOffset(void *data, size_t offset, size_t size)
    {
        if(data && bHostVisible && offset + size <= mSize)//确保写入范围合法
        {
            void *mapping;//映射内存
            VkResult ret = vkMapMemory(mDevice->XJGetDevice(), mBufferMemory, 0, mSize, 0, &mapping);//映射内存
            XJDebug_Log(ret);//将数据复制到指定偏移位置
            if(ret == VK_SUCCESS)
            {
                memcpy(static_cast<char*>(mapping) + offset, data, size);//解除内存映射
                vkUnmapMemory(mDevice->XJGetDevice(), mBufferMemory);//返回结果
            }
            return ret;
        }
        return VK_ERROR_INITIALIZATION_FAILED;
    }
}