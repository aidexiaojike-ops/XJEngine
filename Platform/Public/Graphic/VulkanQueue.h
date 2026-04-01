#ifndef VULKAN_QUEUE_H
#define VULKAN_QUEUE_H

#include "Graphic/VulkanCommon.h"

namespace XJ
{
    class VulkanQueue
    {
        private:

            /* data */
            uint32_t familyIndex = 0; 
            uint32_t index = 0; 
            VkQueue mQueue = VK_NULL_HANDLE;
            bool canPresent = false;


        public:
            VulkanQueue(uint32_t familyIndex, uint32_t index, VkQueue queue, bool canPresent);
            ~VulkanQueue();

            void WaitIdle() const;

            VkQueue XJGetQueue(){return mQueue;}

            void Submit(std::vector<VkCommandBuffer> commandBuffers, const std::vector<VkSemaphore>& waitSemaphores = {}, const std::vector<VkSemaphore>& signalSemaphores = {}, VkFence FrameFence = VK_NULL_HANDLE);//提交命令缓冲区到队列
    };
    
 
    
}

#endif