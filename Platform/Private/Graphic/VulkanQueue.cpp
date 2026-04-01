#include "Graphic/VulkanQueue.h"



namespace XJ
{
    VulkanQueue::VulkanQueue(uint32_t familyIndex, uint32_t index, VkQueue queue, bool canPresent) : familyIndex(familyIndex)
        , index(index)
        , mQueue(queue)
        , canPresent(canPresent)
    {

        spdlog::trace("创建新队列： {0} - {1} - {2}, 现在：{3}", familyIndex, index, (void*) queue, canPresent);
    }
    VulkanQueue::~VulkanQueue()
    {

    }

    void VulkanQueue::WaitIdle() const //等待队列处理完成
    {
        XJDebug_Log(vkQueueWaitIdle(mQueue));
    }   

    void VulkanQueue::Submit(std::vector<VkCommandBuffer> commandBuffers, const std::vector<VkSemaphore>& waitSemaphores, const std::vector<VkSemaphore>& signalSemaphores, VkFence FrameFence)//提交命令缓冲区到队列
    {
        VkPipelineStageFlags WaitDststageMask[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = WaitDststageMask;
        submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        submitInfo.pCommandBuffers = commandBuffers.data();
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
        submitInfo.pSignalSemaphores = signalSemaphores.data();
        
        XJDebug_Log(vkQueueSubmit(mQueue, 1, &submitInfo, FrameFence));
    }

}