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

    bool XJRenderer::XJRendererBegin(int32_t *outImageIndex, std::vector<VkCommandBuffer> mCommandBuffers)
    {
        XJ::XJRenderContext *kRenderCxt = XJ::XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice = kRenderCxt->XJGetDevice();
        XJ::XJVulkanSwapchain *kSwapchain = kRenderCxt->XJGetSwapchain();
     
        bool bShouldUpdateTarget = false;
        //swapchain 重新创建  render target也做对应的更新
        
        // 等待上一帧的提交围栏完成
        XJDebug_Log(vkWaitForFences(kDevice->XJGetDevice(), 1, &mSubmitFences[mCurrentBuffer], VK_TRUE, UINT64_MAX));
        XJDebug_Log(vkResetFences(kDevice->XJGetDevice(), 1, &mSubmitFences[mCurrentBuffer]));
        // 等待图像获取围栏完成（如果存在未完成的获取操作）
        if (mAcquireFences[mCurrentBuffer] != VK_NULL_HANDLE)
        {
            XJDebug_Log(vkWaitForFences(kDevice->XJGetDevice(), 1, &mAcquireFences[mCurrentBuffer], VK_TRUE, UINT64_MAX));
            XJDebug_Log(vkResetFences(kDevice->XJGetDevice(), 1, &mAcquireFences[mCurrentBuffer]));
        }

        //交换链 获取图片
        VkResult kResult = kSwapchain->AcquireImage(outImageIndex, mImageAvailableSemaphores[mCurrentBuffer], mAcquireFences[mCurrentBuffer]);
        if(kResult == VK_ERROR_OUT_OF_DATE_KHR)//交换链过期 需要重建
        {
            XJDebug_Log(vkDeviceWaitIdle(kDevice->XJGetDevice()));//device wait idle 等待设备空闲
            VkExtent2D kOriginExtent = {kSwapchain->XJGetWidth(), kSwapchain->XJGetHeight()};
            
             //重建交换链
            bool bSuc = kSwapchain->ReCreate();
            VkExtent2D kNewExtent = {kSwapchain->XJGetWidth(), kSwapchain->XJGetHeight()};
            if(bSuc && (kOriginExtent.width != kNewExtent.width || kOriginExtent.height != kNewExtent.height))
            {
              
                bShouldUpdateTarget = true;
                spdlog::info("{0}: 交换链重建成功，新的分辨率：{1}x{2}", __FUNCTION__, kNewExtent.width, kNewExtent.height);
            }
            // 在交换链重建后也需要修改
            kResult = kSwapchain->AcquireImage(outImageIndex, mImageAvailableSemaphores[mCurrentBuffer], mAcquireFences[mCurrentBuffer]);
            
            if(kResult != VK_SUCCESS && kResult != VK_SUBOPTIMAL_KHR)
            {
                spdlog::error("{0}: 获取交换链图片失败，错误码：{1}", __FUNCTION__, vk_result_string(kResult));
                return false;
            }

            uint32_t newImageCount = kSwapchain->XJGetSwapchainImages().size();
            if(newImageCount != mCommandBuffers.size())
            {
                
            }
        }
        else if(kResult != VK_SUCCESS && kResult != VK_SUBOPTIMAL_KHR)
        {
            spdlog::error("{0}: 获取交换链图片失败，错误码：{1}", __FUNCTION__, vk_result_string(kResult));
            return false;
        }
        if (*outImageIndex < 0 || *outImageIndex >= static_cast<int32_t>(mCommandBuffers.size())) 
        {
            spdlog::error("无效的图像索引: {}", *outImageIndex);
            return false;
        }

        return bShouldUpdateTarget;
    }

    

    bool XJRenderer::XJRendererEnd(int32_t imageIndex, const std::vector<VkCommandBuffer> &cmdBuffers)
    {
        XJ::XJRenderContext *kRenderCxt = XJ::XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice = kRenderCxt->XJGetDevice();
        XJ::XJVulkanSwapchain *kSwapchain = kRenderCxt->XJGetSwapchain();
        bool bShouldUpdateTarget = false;

        kDevice->XJGetFirstGraphicQueue()->Submit(cmdBuffers, { mImageAvailableSemaphores[mCurrentBuffer] }, { mSubmitedSemaphores[mCurrentBuffer] }, mSubmitFences[mCurrentBuffer]);
        //显示 presen
        VkResult ret = kSwapchain->Present(imageIndex, { mSubmitedSemaphores[mCurrentBuffer] });
        if(ret == VK_SUBOPTIMAL_KHR)
        {
           XJDebug_Log(vkDeviceWaitIdle(kDevice->XJGetDevice()));//device wait idle 等待设备空闲
           VkExtent2D originExtent = { kSwapchain->XJGetWidth(), kSwapchain->XJGetHeight() };
           bool bSuc = kSwapchain->ReCreate();
            //重建交换链
           VkExtent2D newExtent = { kSwapchain->XJGetWidth(), kSwapchain->XJGetHeight() };//更新渲染目标的分辨率
           if(bSuc && (originExtent.width != newExtent.width || originExtent.height != newExtent.height))
           {
               bShouldUpdateTarget = true;
               
           }
        }
        XJDebug_Log(vkDeviceWaitIdle(kDevice->XJGetDevice()));//等待每一帧结束之后
        mCurrentBuffer = (mCurrentBuffer + 1) % RENDERER_NUM_BUFFER;
        return bShouldUpdateTarget;
    
    }


}