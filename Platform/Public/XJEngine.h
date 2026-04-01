/*
#ifndef XJ_ENGINE_H
#define XJ_ENGINE_H

#include "Edit/XJGlfwWindow.h"
#include "Edit/SpdlogDebug.h"
#include "Edit/FileUtil.h"
#include "Edit/Mathinclude.h"
#include "Edit/EditIncludes.h"

#include "Graphic/VulkanInstance.h"
#include "Graphic/VulkanSurface.h"
#include "Graphic/VulkanPhysicalDevices.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/XJVulkanSwapchain.h"
#include "Graphic/VulkanQueue.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/XJVulkanCommandBuffer.h"
#include "Graphic/XJVulkanImage.h"
#include "Graphic/XJVulkanGeometryUtil.h"
#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanDepthImage.h"

#include <chrono>


//#include <vulkan/vulkan.h>
//#include <vk_video/vulkan_video_codec_h264std.h>
// 正确写法：

namespace XJ
{

    struct PushConstants
    {
         glm::mat4 matrix{1.0f}; // 4x4 矩阵，默认初始化为单位矩阵
    };// 推送常量结构体
   
    class XJEngine
    {
        private:
           
            bool isRuning = true;

            std::unique_ptr<XJGlfwWindow>      mWindow;
            std::unique_ptr<SpdlogDebug>       mSpdlogDebug;
            std::unique_ptr<VulkanInstance>    mInstance;
            std::unique_ptr<VulkanSurface>     mSurface;
            std::unique_ptr<VulkanPhysicalDevices> mPhysicalDevices;
            std::unique_ptr<XJVulkanDevice>      mDevice;
            std::unique_ptr<XJVulkanSwapchain>   mSwapchain;

            std::unique_ptr<VulkanQueue>   mQueue;//图形队列
            XJ::VulkanQueue* mGraphicQueue = nullptr;//图形队列指针

            std::unique_ptr<XJVulkanRenderPass>  mRenderPass;
            std::vector<std::shared_ptr<XJ::XJVulkanFrameBuffer>> mFrameBuffers;
            XJ::ShaderLayout mShaderLayout;
            std::vector<std::shared_ptr<XJ::XJVulkanImage>> mImages;
            std::vector<std::shared_ptr<XJ::XJVulkanDepthImage>> mDepthImages;
            std::unique_ptr<XJVulkanPipelineLayout> mPipelineLayout;
            std::unique_ptr<XJVulkanPipeline>    mPipeline;
            std::unique_ptr<XJVulkanCommandPool> mCommandPool;
            std::vector<VkCommandBuffer>      mCommandBuffers;

            // 清除值在运行时设置，因为不需要深度附件
            std::vector<VkClearValue> clearValues = 
            {
                VkClearValue{.color = {{0.5f, 0.5f, 0.5f, 1.0f}}}, // 清除颜色附件为黑色
                VkClearValue{.depthStencil = {1.0f, 0}}          // 清除深度附件为1.0，模板附件为0
            };
            //std::vector<VkClearValue> clearValues;
            VkFence acquireFence;

          
            std::vector<VkSemaphore> mImageAvailableSemaphores;//图像可用信号量
            std::vector<VkSemaphore> mSubimedSemaphores;//提交信号量
            std::vector<VkFence> mFrameFences;//帧围栏
            
            std::vector<VkImage> mSwapchainImages;//交换链图片数组
            //geometry util
            std::vector<XJ::XJVulkanVertex> mVertices;
            std::vector<uint32_t> mIndices;
            std::unique_ptr<XJVulkanGeometryUtil> mGeometryUtil;
            std::shared_ptr<XJVulkanBuffer> mVertexBuffer;//顶点缓冲区
            std::shared_ptr<XJVulkanBuffer> mIndexBuffer;//索引缓冲区

        public:
            uint32_t kNumBuffer = 2; // 缓冲区数量
            uint32_t kCurrentBuffer = 0; // 当前缓冲区索引
            uint32_t kSwapchainImageSize = 0; // 交换链图片数量
            VkExtent3D kImageExtent{}; // 交换链图片的宽度和高度
            VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT; // 深度格式
            
            PushConstants kPC{};//推送常量实例

           std::chrono::time_point<std::chrono::steady_clock> kLastTimePoint
            = std::chrono::steady_clock::now();
            
            XJEngine();
            ~XJEngine();

            void EngineRun();
          
            
            bool EndEngine();

    };

}

#endif*/