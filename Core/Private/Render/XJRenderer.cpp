#include "Render/XJRenderer.h"

#include "XJApplication.h"
#include "Graphic/VulkanQueue.h"
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
        mAcquireFences.resize(RENDERER_NUM_BUFFER);     // 新增：图像获取围栏
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
            XJDebug_Log(vkCreateFence(kDevice->XJGetDevice(), &fenceInfo, nullptr, &mAcquireFences[i]));  // 新增
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
            vkDestroyFence(kDevice->XJGetDevice(), mAcquireFences[i], nullptr);  // 新增
            vkDestroyFence(kDevice->XJGetDevice(), mSubmitFences[i], nullptr);   // 修改
        }
    }

    XJFrameAcquireResult XJRenderer::XJRendererBegin(const std::vector<VkCommandBuffer>& commandBuffers)
    {
        XJFrameAcquireResult result{};

        XJ::XJRenderContext *kRenderCxt = XJ::XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice = kRenderCxt->XJGetDevice();
        XJ::XJVulkanSwapchain *kSwapchain = kRenderCxt->XJGetSwapchain();
        
        // 等待上一帧的提交围栏完成
        VkResult fenceResult = vkWaitForFences(kDevice->XJGetDevice(), RENDERER_NUM_BUFFER, mSubmitFences.data(), VK_TRUE, UINT64_MAX);//等待上一帧的提交围栏完成
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

        if (mAcquireFences[mCurrentBuffer] != VK_NULL_HANDLE)
        {
            // 等待图像获取围栏完成（如果存在未完成的获取操作）
            VkResult acquireFenceWait = vkWaitForFences(
                kDevice->XJGetDevice(),
                1,
                &mAcquireFences[mCurrentBuffer],
                VK_TRUE,
                UINT64_MAX);
            
            if (acquireFenceWait != VK_SUCCESS)
            {
                spdlog::error("Wait acquire fence failed: {}", vk_result_string(acquireFenceWait));
                return result;
            }
        
            XJDebug_Log(vkResetFences(kDevice->XJGetDevice(), 1, &mAcquireFences[mCurrentBuffer]));
        }

        int32_t imageIndex = -1;
        //交换链 获取图片
        VkResult acquireResult = kSwapchain->AcquireImage(&imageIndex, mImageAvailableSemaphores[mCurrentBuffer], mAcquireFences[mCurrentBuffer]);
        //spdlog::trace("STEP2: AcquireImage returned {}", vk_result_string(kResult));  
        if(acquireResult == VK_ERROR_OUT_OF_DATE_KHR)//交换链过期 需要重建
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
                &imageIndex,
                mImageAvailableSemaphores[mCurrentBuffer],
                mAcquireFences[mCurrentBuffer]);
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        {
            spdlog::error("{}: 获取交换链图片失败，错误码：{}", __FUNCTION__, vk_result_string(acquireResult));
            return result;
        }

        if (acquireResult == VK_SUBOPTIMAL_KHR)
        {
            result.resizeNeeded = true;
        }

        if (imageIndex < 0 || imageIndex >= static_cast<int32_t>(commandBuffers.size())) 
        {
            spdlog::error("无效的图像索引: {}", imageIndex);
            return result;
        }

        result.acquired = true;
        result.imageIndex = imageIndex;
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
        
        XJDebug_Log(vkResetFences(kDevice->XJGetDevice(), 1, &mSubmitFences[mCurrentBuffer]));

        kDevice->XJGetFirstGraphicQueue()->Submit(cmdBuffers, { mImageAvailableSemaphores[mCurrentBuffer] }, { mSubmitedSemaphores[mCurrentBuffer] }, mSubmitFences[mCurrentBuffer]);
        //显示 presen
        VkResult presentResult  = kSwapchain->Present(imageIndex, { mSubmitedSemaphores[mCurrentBuffer] });

        if(presentResult  == VK_ERROR_DEVICE_LOST)
        {
            spdlog::critical("Present 失败：设备丢失 (VK_ERROR_DEVICE_LOST)");
            return result;
        }

        if(presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
        {
            XJDebug_Log(vkDeviceWaitIdle(kDevice->XJGetDevice()));//device wait idle 等待设备空闲
            VkExtent2D oldExtent  = { kSwapchain->XJGetWidth(), kSwapchain->XJGetHeight() };
            bool recreated  = kSwapchain->ReCreate();
             //重建交换链
            VkExtent2D newExtent = { kSwapchain->XJGetWidth(), kSwapchain->XJGetHeight() };//更新渲染目标的分辨率
            result.resizeNeeded = recreated &&
                (oldExtent.width != newExtent.width || oldExtent.height != newExtent.height);
        }
        else if(presentResult  != VK_SUCCESS)
        {
            spdlog::error("Present 失败：{}", vk_result_string(presentResult ));
            return result;
        }
        //latform/Private/Graphic/XJVulkanSwapchain.cpp，第 192-194 行两处 WaitIdle 让每帧变成完全同步（等 GPU 跑完才进入下一帧），严重拖慢性能。
        // XJDebug_Log(vkDeviceWaitIdle(kDevice->XJGetDevice()));//等待每一帧结束之后
        mCurrentBuffer = (mCurrentBuffer + 1) % RENDERER_NUM_BUFFER;

        result.presented = true;
        return result;
    
    }


}