#include "Render/XJRenderer.h"

#include "XJApplication.h"
#include "Graphic/XJVulkanQueue.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/VulkanCommon.h"
#include "Graphic/XJVulkanBuffer.h"
#include "Render/XJRenderTarget.h"


namespace XJ
{
    XJRenderer::XJRenderer(/* args */)
    {
        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();
         // 创建同步对象
        mImageAvailableSemaphores.resize(RENDERER_NUM_BUFFER);
        mSubmitedSemaphores.resize(RENDERER_NUM_BUFFER);
        // mAcquireFences.resize(RENDERER_NUM_BUFFER);     // 新增：图像获取围栏
        mSubmitFences.resize(RENDERER_NUM_BUFFER);      // 修改：队列提交围栏
        XJ::PipelineRasterizationState rasterState;
        //rasterState.cullMode = VK_CULL_MODE_NONE;  // 禁用剔除
        //mPipeline->SetRasterizationState(rasterState);
        // 创建信号量
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreInfo.pNext = nullptr;
        semaphoreInfo.flags = 0;
        // 创建围栏，初始状态为有信号量
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.pNext = nullptr;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < RENDERER_NUM_BUFFER; i++)
        {
            XJDebug_Log(vkCreateSemaphore(kDevice->XJGetDevice(), &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]));//创建信号量
            XJDebug_Log(vkCreateSemaphore(kDevice->XJGetDevice(), &semaphoreInfo, nullptr, &mSubmitedSemaphores[i]));//创建信号量
            // XJDebug_Log(vkCreateFence(kDevice->XJGetDevice(), &fenceInfo, nullptr, &mAcquireFences[i]));  // 新增
            XJDebug_Log(vkCreateFence(kDevice->XJGetDevice(), &fenceInfo, nullptr, &mSubmitFences[i]));   // 修改
        }
    }
    
    XJRenderer::~XJRenderer()
    {
        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();
        for(int i = 0; i < RENDERER_NUM_BUFFER; i++)
        {
            vkDestroySemaphore(kDevice->XJGetDevice(), mImageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(kDevice->XJGetDevice(), mSubmitedSemaphores[i], nullptr);
            // vkDestroyFence(kDevice->XJGetDevice(), mAcquireFences[i], nullptr);  // 新增
            vkDestroyFence(kDevice->XJGetDevice(), mSubmitFences[i], nullptr);   // 修改
        }
    }

    bool XJRenderer::RecreateSignaledFence(XJVulkanDevice* device, VkFence& fence)//重建已信号的围栏
    {
        if (!device || !device->IsValid())
            return false;

        VkDevice vkDevice = device->XJGetDevice();

        // submit 失败后，刚 reset 的 fence 不会被 GPU signal。
        // Vulkan 不能手动 signal fence，所以这里销毁并重建一个已信号的 fence，避免下一帧永久等待。
        if (fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(vkDevice, fence, nullptr);
            fence = VK_NULL_HANDLE;
        }

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
         // 等待图像获取围栏完成（如果存在未完成的获取操作）
        VkResult ret = vkCreateFence(vkDevice, &fenceInfo, nullptr, &fence);
        if (ret != VK_SUCCESS)
        {
            spdlog::critical("Failed to recreate signaled fence: {}", vk_result_string(ret));
            return false;
        }

        return true;
    }

    XJFrameAcquireResult XJRenderer::XJRendererBegin(const std::vector<VkCommandBuffer>& commandBuffers)
    {
        XJFrameAcquireResult result{};

        XJ::XJRenderContext *kRenderCxt = XJ::XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice = kRenderCxt->XJGetDevice();
        XJ::XJVulkanSwapchain *kSwapchain = kRenderCxt->XJGetSwapchain();
        XJApplication::XJGetAppContext()->renderFrameSlot = mCurrentBuffer;
        
        // 等待上一帧的提交围栏完成  
        //VkResult fenceResult = vkWaitForFences(kDevice->XJGetDevice(), RENDERER_NUM_BUFFER, mSubmitFences.data(), VK_TRUE, UINT64_MAX);//等待上一帧的提交围栏完成
        //排除陷入无线循环
        constexpr uint64_t FENCE_WAIT_TIMEOUT_NS = 1'000'000'000; // 1 second

        VkFence& submitFence = mSubmitFences[mCurrentBuffer];

        // 只等待当前 CPU 即将复用的帧槽位。
        // 不等待其他槽位，CPU 可以开始录制下一帧，让双缓冲真正并行。
        VkResult fenceResult = vkWaitForFences(
            kDevice->XJGetDevice(),
            1,
            &submitFence,
            VK_TRUE,
            FENCE_WAIT_TIMEOUT_NS);
        
        if (fenceResult == VK_TIMEOUT)
        {
            spdlog::error(
                "Wait submit fence timeout, frameSlot={}. A previous submit may not have signaled.",
                mCurrentBuffer);
            return result;
        }

        if(fenceResult == VK_ERROR_DEVICE_LOST)
        {
            spdlog::critical("WaitForFences: 设备丢失，无法继续渲染");
            return result;
        }
        if (fenceResult != VK_SUCCESS)
        {
            spdlog::error("WaitForFences failed: {}", vk_result_string(fenceResult));
            return result;
        }

      

        //交换链 获取图片
        XJSwapchainAcquireResult acquireResult =
                kSwapchain->AcquireImage(mImageAvailableSemaphores[mCurrentBuffer]);
        if (acquireResult.result == VK_ERROR_DEVICE_LOST)
        {
            spdlog::critical("AcquireImage 失败：设备丢失");
            return result;
        }

        if(acquireResult.recreateNeeded && !acquireResult.acquired)//交换链过期 需要重建
        {
            XJDebug_Log(vkDeviceWaitIdle(kDevice->XJGetDevice()));//device wait idle 等待设备空闲

            VkExtent2D oldExtent  = {kSwapchain->XJGetWidth(), kSwapchain->XJGetHeight()};
             //重建交换链
            bool recreated  = kSwapchain->ReCreate();
            VkExtent2D newExtent  = {kSwapchain->XJGetWidth(), kSwapchain->XJGetHeight()};

            result.resizeNeeded = recreated &&
                (oldExtent.width != newExtent.width || oldExtent.height != newExtent.height);
            
            if (!recreated)
            {
                spdlog::error("{}: 交换链重建失败", __FUNCTION__);
                return result;
            }

           acquireResult = kSwapchain->AcquireImage(
                mImageAvailableSemaphores[mCurrentBuffer]);
        }
        if (!acquireResult.acquired)
        {
            spdlog::error("{}: 获取交换链图片失败，错误码：{}", __FUNCTION__, vk_result_string(acquireResult.result));
            return result;
        }

        if (acquireResult.recreateNeeded)
        {
            result.resizeNeeded = true;
        }
        uint32_t imageIndex = acquireResult.imageIndex;
        // image index 校验
        if (imageIndex >= commandBuffers.size())
        {
            spdlog::error("无效的图像索引: {}", imageIndex);
            return result;
        }


        result.acquired = true;
        result.imageIndex = static_cast<int32_t>(imageIndex);
        return result;
    }

    

    XJFramePresentResult  XJRenderer::XJRendererEnd(int32_t imageIndex, const std::vector<VkCommandBuffer> &cmdBuffers)
    {
        XJFramePresentResult result{};

        XJ::XJRenderContext *kRenderCxt = XJ::XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice = kRenderCxt->XJGetDevice();
        XJ::XJVulkanSwapchain *kSwapchain = kRenderCxt->XJGetSwapchain();

        if (imageIndex < 0)
        {
            spdlog::error("Present skipped: invalid image index {}", imageIndex);
            return result;
        }

        VkFence& submitFence = mSubmitFences[mCurrentBuffer];

        VkResult resetRet = vkResetFences(kDevice->XJGetDevice(), 1, &submitFence);
        if (resetRet != VK_SUCCESS)
        {
            spdlog::error("Reset submit fence failed: {}", vk_result_string(resetRet));
            return result;
        }

        XJVulkanQueue* graphicsQueue = kDevice->XJGetFirstGraphicQueue();
        if (!graphicsQueue)
        {
            spdlog::error("Submit skipped: graphics queue is null");
        
            // fence 已经 reset，如果直接返回，下一帧会永久等待它。
            RecreateSignaledFence(kDevice, submitFence);
            return result;
        }

        VkResult submitRet = graphicsQueue->Submit(
            cmdBuffers,
            { mImageAvailableSemaphores[mCurrentBuffer] },
            { mSubmitedSemaphores[mCurrentBuffer] },
            submitFence);
        
        if (submitRet != VK_SUCCESS)
        {
            spdlog::error("Frame submit failed: {}", vk_result_string(submitRet));
        
            // vkQueueSubmit 失败时 fence 不会被 signal；恢复为 signaled，避免下一帧 vkWaitForFences 卡死。
            RecreateSignaledFence(kDevice, submitFence);
            return result;
        }
        //显示 presen
        XJSwapchainPresentResult  presentResult  = kSwapchain->Present(static_cast<uint32_t>(imageIndex), { mSubmitedSemaphores[mCurrentBuffer] });

        if (presentResult.result == VK_ERROR_DEVICE_LOST)
        {
            spdlog::critical("Present 失败：设备丢失");
            return result;
        }

        if(presentResult.recreateNeeded)
        {
            XJDebug_Log(vkDeviceWaitIdle(kDevice->XJGetDevice()));//device wait idle 等待设备空闲

            VkExtent2D oldExtent  = { kSwapchain->XJGetWidth(), kSwapchain->XJGetHeight() };
            bool recreated  = kSwapchain->ReCreate();
             //重建交换链
            VkExtent2D newExtent = { kSwapchain->XJGetWidth(), kSwapchain->XJGetHeight() };//更新渲染目标的分辨率
            result.resizeNeeded = recreated &&
                (oldExtent.width != newExtent.width || oldExtent.height != newExtent.height);

            if (!recreated)
            {
                spdlog::error("{}: 交换链重建失败", __FUNCTION__);
                return result;
            }
        }
        else if(!presentResult.presented)
        {
            spdlog::error("Present 失败：{}", vk_result_string(presentResult.result));
            return result;
        }
        //latform/Private/Graphic/XJVulkanSwapchain.cpp，第 192-194 行两处 WaitIdle 让每帧变成完全同步（等 GPU 跑完才进入下一帧），严重拖慢性能。
        // XJDebug_Log(vkDeviceWaitIdle(kDevice->XJGetDevice()));//等待每一帧结束之后
        mCurrentBuffer = (mCurrentBuffer + 1) % RENDERER_NUM_BUFFER;

        result.presented = true;
        return result;
    
    }


}
