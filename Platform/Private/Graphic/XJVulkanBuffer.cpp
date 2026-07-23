#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/VulkanQueue.h"
#include "Graphic/XJVulkanCommandBuffer.h"
#include <stdexcept>

namespace XJ
{
    
    XJVulkanBuffer::XJVulkanBuffer(XJVulkanDevice* device, VkBufferUsageFlags usage, size_t size,const void* data, bool bHostVisible)
                : mDevice(device), mSize(size), bHostVisible(bHostVisible)
    {
       CreateBuffer(usage, data);
    }
    XJVulkanBuffer::~XJVulkanBuffer()
    {
        if (!mDevice || !mDevice->IsValid())
        {
            return;
        }

        mDevice->WaitIdle();

        if (mBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(mDevice->XJGetDevice(), mBuffer, nullptr);
            mBuffer = VK_NULL_HANDLE;
        }

        if (mBufferMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(mDevice->XJGetDevice(), mBufferMemory, nullptr);
            mBufferMemory = VK_NULL_HANDLE;
        }
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
        int32_t memoryTypeIndex = device->XJGetMemoryIndex(memoryPropertyFlags, memRequirements.memoryTypeBits);
        if (memoryTypeIndex < 0)
        {
            spdlog::error(
                "创建 Buffer 失败：找不到合适的内存类型，requiredFlags=0x{:X}, memoryTypeBits=0x{:X}",
                memoryPropertyFlags,
                memRequirements.memoryTypeBits);
            
            vkDestroyBuffer(device->XJGetDevice(), *outBuffer, nullptr);
            *outBuffer = VK_NULL_HANDLE;
            *outBufferMemory = VK_NULL_HANDLE;
            
            throw std::runtime_error("XJVulkanBuffer::CreateBufferInternal failed: no suitable memory type");
        }
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = static_cast<uint32_t>(memoryTypeIndex);
    
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
            if(!data)
            {
                CreateBufferInternal(
                    mDevice,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    mSize,
                    &mBuffer,
                    &mBufferMemory);
                return;
            }

            //创建临时缓冲区用于数据传输
            VkBuffer stagingBuffer = VK_NULL_HANDLE;
            VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
             //创建临时缓冲区
            try
            {
                //创建临时缓冲区用于数据传输
                CreateBufferInternal(mDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, mSize, &stagingBuffer, &stagingBufferMemory);
    
                //将数据复制到临时缓冲区
                void *mapping = nullptr;
                XJDebug_Log(vkMapMemory(mDevice->XJGetDevice(), stagingBufferMemory, 0, mSize, 0, &mapping));
                memcpy(mapping, data, mSize);
                vkUnmapMemory(mDevice->XJGetDevice(), stagingBufferMemory);

                //创建实际使用的缓冲区
                CreateBufferInternal(
                    mDevice,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    mSize,
                    &mBuffer,
                    &mBufferMemory);
                
                //只有存在初始数据时，才从 staging 拷贝到 device buffer
                CopyToBuffer(mDevice, stagingBuffer, mBuffer, mSize);
                
                vkDestroyBuffer(mDevice->XJGetDevice(), stagingBuffer, nullptr);
                vkFreeMemory(mDevice->XJGetDevice(), stagingBufferMemory, nullptr);

                stagingBuffer = VK_NULL_HANDLE;
                stagingBufferMemory = VK_NULL_HANDLE;
            }
            catch (...)
            {
                if (stagingBuffer != VK_NULL_HANDLE)
                {
                    vkDestroyBuffer(mDevice->XJGetDevice(), stagingBuffer, nullptr);
                    stagingBuffer = VK_NULL_HANDLE;
                }
            
                if (stagingBufferMemory != VK_NULL_HANDLE)
                {
                    vkFreeMemory(mDevice->XJGetDevice(), stagingBufferMemory, nullptr);
                    stagingBufferMemory = VK_NULL_HANDLE;
                }
            
                throw;
            }

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
