#ifndef XJ_RENDER_TARGET_H
#define XJ_RENDER_TARGET_H


#include "Graphic/XJVulkanFrameBuffer.h"
#include "XJRenderContext.h"
#include "ECS/XJSystem.h"
#include "ECS/XJEntity.h"

namespace XJ
{
    //class XJVulkanRenderPass;

    class XJRenderTarget
    {
        private:
            void Init();
            void ReCreate();

            std::vector<std::shared_ptr<XJVulkanFrameBuffer>> mFrameBuffers;
            std::vector<std::shared_ptr<XJMaterialSystem>> mMaterialSystemList;

            XJVulkanRenderPass *mRenderPass = nullptr;
            std::vector<VkClearValue> mClearValues;//清除值数组
            uint32_t mBufferCount;
            uint32_t mCurrentBufferIndex = 0;//当前帧缓冲索引
            VkExtent2D mExtent;
    
            bool bSwapchainTarget = false; // 标志是否为交换链目标
            bool bBeginRenderTarget = false; // 标志是否已开始渲染目标

            XJEntity *mCamera = nullptr;//摄像机事件

            bool bShouldUpdate = false; // 标志是否需要更新帧缓冲（例如窗口大小改变时） 
             /* data */

            std::vector<std::shared_ptr<XJVulkanImage>> mColorImages;//颜色附件图像列表
            std::vector<std::shared_ptr<XJ::XJVulkanDepthImage>> mDepthImages;//深度附件图像列表


        public:
            XJRenderTarget(XJVulkanRenderPass *rederPass);//构造函数重载，允许直接传入帧缓冲数量和尺寸
            XJRenderTarget(XJVulkanRenderPass *rederPass,uint32_t bufferCount, VkExtent2D extent);//构造函数重载，允许直接传入帧缓冲数量和尺寸
            ~XJRenderTarget();
           

            void BeginRenderTarget(VkCommandBuffer commandBuffer);//开始渲染目标
            void EndRenderTarget(VkCommandBuffer commandBuffer);//结束渲染目标

            XJVulkanRenderPass *XJGetRenderPass() const { return mRenderPass; }
            XJVulkanFrameBuffer* XJGetCurrentFrameBuffer() const {  if (mFrameBuffers.empty()) return nullptr; if (mCurrentBufferIndex >= mFrameBuffers.size()) return nullptr; return mFrameBuffers[mCurrentBufferIndex].get();  }

            void SetExtent(const VkExtent2D extent);
            VkExtent2D GetExtent()const {return mExtent;}

            void SetBufferCount(uint32_t bufferCount);

            void SetColorClearValue(VkClearColorValue colorClearValue);//设置颜色清除值
            void SetColorClearValue(uint32_t attachmentIndex, VkClearColorValue colorClearValue);
            void SetDepthClearValue(VkClearDepthStencilValue depthClearValue);//设置深度清除值
            void SetDepthClearValue(uint32_t attachmentIndex, VkClearDepthStencilValue depthClearValue);

            template<typename T, typename... Args>
            void AddMaterialSystem(Args&&... args)
            {
                std::shared_ptr<XJMaterialSystem> system = std::make_shared<T>(std::forward<Args>(args)...);//实例化材质系统
                system->OnInit(mRenderPass);//初始化
                mMaterialSystemList.push_back(system);//添加
            }

            void RenderMaterialSystem(VkCommandBuffer cmdBuffer)
            {
                for(auto &item: mMaterialSystemList)//便利所有的材质系统
                {
                    item->OnRender(cmdBuffer, this);//渲染
                }
            }

            void XJSetCamera(XJEntity *camera) { mCamera = camera; }
            XJEntity *XJGetCamera() const { return mCamera; }

    };
}
#endif