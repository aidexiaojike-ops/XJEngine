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

    void VulkanQueue::Submit(
        std::vector<VkCommandBuffer> commandBuffers,
        const std::vector<VkSemaphore>& waitSemaphores,
        const std::vector<VkSemaphore>& signalSemaphores,
        VkFence FrameFence,
        const std::vector<VkPipelineStageFlags>& waitStageMasks)//提交命令缓冲区到队列
    {
        //spdlog::warn("Queue Submit");
        const char* caller = (FrameFence != VK_NULL_HANDLE) ? "PER-FRAME" : "ONE-TIME";
        /*spdlog::debug("Submit [{}]: cmdBuf={}, waitSem={}, sigSem={}, fence={}",
            caller, commandBuffers.size(), waitSemaphores.size(),
            signalSemaphores.size(), (void*)FrameFence);*/

        std::vector<VkPipelineStageFlags> effectiveWaitStageMasks;
        if (!waitSemaphores.empty())
        {
            if (!waitStageMasks.empty() && waitStageMasks.size() == waitSemaphores.size())
            {
                effectiveWaitStageMasks = waitStageMasks;
            }
            else
            {
                if (!waitStageMasks.empty())
                {
                    spdlog::warn(
                        "Submit [{}]: waitStageMasks size ({}) does not match waitSemaphores size ({}). Using default stage masks.",
                        caller,
                        waitStageMasks.size(),
                        waitSemaphores.size());
                }

                effectiveWaitStageMasks.resize(
                    waitSemaphores.size(),
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            }
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        submitInfo.pWaitSemaphores = waitSemaphores.empty() ? nullptr : waitSemaphores.data();
        submitInfo.pWaitDstStageMask = effectiveWaitStageMasks.empty() ? nullptr : effectiveWaitStageMasks.data();
        submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        submitInfo.pCommandBuffers = commandBuffers.empty() ? nullptr : commandBuffers.data();
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
        submitInfo.pSignalSemaphores = signalSemaphores.empty() ? nullptr : signalSemaphores.data();
        
        VkResult ret = vkQueueSubmit(mQueue, 1, &submitInfo, FrameFence);
        if (ret != VK_SUCCESS)
        {
            spdlog::error("Submit [{}] FAILED: {}", caller, vk_result_string(ret));
        }
        XJDebug_Log(ret);
    }

}
