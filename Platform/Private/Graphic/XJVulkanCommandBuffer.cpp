#include "Graphic/XJVulkanCommandBuffer.h"
#include "Graphic/XJVulkanDevice.h"

namespace XJ
{
    XJVulkanCommandPool::XJVulkanCommandPool(XJVulkanDevice* device, uint32_t queueFamilyIndex) : mDevice(device)
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo{};//命令池创建信息
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.pNext = nullptr;
        commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;//命令池关联的队列族索引
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;//允许单个命令缓冲区重置

        XJDebug_Log(vkCreateCommandPool(mDevice->XJGetDevice(), &commandPoolCreateInfo, nullptr, &mCommandPool));
        spdlog::trace("命令池创建成功: {}", (void*)mCommandPool);
        
    }

    XJVulkanCommandPool::~XJVulkanCommandPool()
    {
        if (mCommandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(mDevice->XJGetDevice(), mCommandPool, nullptr);
    }

    std::vector<VkCommandBuffer> XJVulkanCommandPool::AllocateCommandBuffer(uint32_t count) const//分配命令缓冲区
    {
        spdlog::trace("分配了 {} 个命令缓冲区", count);
        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.pNext = nullptr;
        commandBufferAllocateInfo.commandPool = mCommandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = count;

        std::vector<VkCommandBuffer> commandBuffers(count);
        XJDebug_Log(vkAllocateCommandBuffers(mDevice->XJGetDevice(), &commandBufferAllocateInfo, commandBuffers.data()));

       
        return commandBuffers;
    }//生气commandbuffer
    
    VkCommandBuffer XJVulkanCommandPool::AllocateSingleCommandBuffer() const//分配单个命令缓冲区
    {
        std::vector<VkCommandBuffer> commandBuffers = AllocateCommandBuffer(1);
        return commandBuffers[0];
    }

    void XJVulkanCommandPool::BeginCommandBuffer(VkCommandBuffer commandBuffer)//开始记录命令
    {
        XJDebug_Log(vkResetCommandBuffer(commandBuffer, 0));

        VkCommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.pNext = nullptr;
        commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        commandBufferBeginInfo.pInheritanceInfo = nullptr;

        XJDebug_Log(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    }
    void XJVulkanCommandPool::EndCommandBuffer(VkCommandBuffer commandBuffer)//结束命令记录
    {
        XJDebug_Log(vkEndCommandBuffer(commandBuffer));
    }


}