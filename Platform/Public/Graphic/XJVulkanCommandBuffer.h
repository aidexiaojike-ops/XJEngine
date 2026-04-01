#ifndef XJ_VULKAN_COMMANDBUFFER_H
#define XJ_VULKAN_COMMANDBUFFER_H

#include "Graphic/VulkanCommon.h"

namespace XJ
{
    class XJVulkanDevice;
    class XJVulkanCommandPool//所有 shader 都会用到的“公共参数缓冲”时间  帧号 分辨率 相机矩阵 光照参数 全局开关
    {
        public:
            XJVulkanCommandPool(XJVulkanDevice* device, uint32_t queueFamilyIndex);
            ~XJVulkanCommandPool();
        
            static void BeginCommandBuffer(VkCommandBuffer commandBuffer);
            static void EndCommandBuffer(VkCommandBuffer commandBuffer);

            std::vector<VkCommandBuffer> AllocateCommandBuffer(uint32_t count) const;//分配命令缓冲区
            VkCommandBuffer AllocateSingleCommandBuffer() const;//分配单个命令缓冲区
            

            VkCommandBuffer XJGetCommandBuffer() const { return mCommandBuffer; }
            VkCommandPool XJGetCommandPool() const { return mCommandPool; }

        private:
            XJVulkanDevice* mDevice = nullptr;
            VkCommandPool mCommandPool = VK_NULL_HANDLE;
            VkCommandBuffer mCommandBuffer = VK_NULL_HANDLE;
    };
}



#endif