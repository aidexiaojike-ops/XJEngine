#ifndef XJ_RENDERER_CPP
#define XJ_RENDERER_CPP

#include "Render/XJRenderContext.h"

namespace XJ
{
    class XJVulkanPipeline;
    class XJRenderTarget;

    #define RENDERER_NUM_BUFFER  2 //双buffer

    class XJRenderer
    {
        private:
            /* data */

            uint32_t mCurrentBuffer = 0; // 当前缓冲区索引

            VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT; // 多重采样数量

            std::vector<VkSemaphore> mImageAvailableSemaphores;//图像可用信号量
            std::vector<VkSemaphore> mSubmitedSemaphores;//提交信号量
            std::vector<VkFence> mAcquireFences;  // 用于图像获取
            std::vector<VkFence> mSubmitFences;   // 用于队列提交

            //std::vector<VkFence> mFrameFences;
        public:
            XJRenderer(/* args */);
            ~XJRenderer();
          
            bool XJRendererBegin(int32_t *outImageIndex, std::vector<VkCommandBuffer> mCommandBuffers);
            bool XJRendererEnd(int32_t imageIndex, const std::vector<VkCommandBuffer> &cmdBuffers);
    };
    
  
    
}

#endif