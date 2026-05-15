#ifndef XJ_VIEWPORT_H
#define XJ_VIEWPORT_H

#include "Graphic/VulkanCommon.h"
#include "Render/XJRenderTarget.h"

#include <memory>
#include <imgui.h>

namespace XJ
{
    class XJRenderContext;
    class XJVulkanRenderPass;
    class XJVulkanDevice;
    class XJEntity;

    struct ViewportSettings
    {
        uint32_t mWidth = 64;
        uint32_t mHeight = 64;

        const char* mViewportName = "Viewport";
    };

    class XJViewport
    {
        public:

            virtual ~XJViewport() = default;

        public:

            virtual bool Init(XJRenderContext* renderContext);
            virtual void Resize(uint32_t width, uint32_t height);
            virtual bool Render(VkCommandBuffer cmd)  = 0;
            virtual void DrawUI();
            //渲染前准备方法
            virtual void PrepareBeforeRender();
            virtual void Shutdown();
            virtual void PostRender();

            template<typename T, typename... Args>
            void AddMaterialSystem(Args&&... args)
            {
                if (mRenderTarget)
                    mRenderTarget->template AddMaterialSystem<T>(std::forward<Args>(args)...);
            }

            void SetViewportName(const char* name) { mSettings.mViewportName = name; }

        public:

            ImTextureID GetTextureID() const
            {
                return (ImTextureID)mDescriptorSet;
            }

            XJRenderTarget* GetRenderTarget() const
            {
                return mRenderTarget.get();
            }

        protected:

             //释放 descriptor 的小工具函数：
            virtual void RecreateDescriptor();
            virtual void ReleaseDescriptor();
            virtual bool BeginViewportRender(VkCommandBuffer cmd);
            virtual void EndViewportRender(VkCommandBuffer cmd);
            virtual void CreateRenderPass(VulkanPhysicalDevices* physicalDevices);

        protected:

            XJRenderContext* mRenderContext = nullptr;

            XJVulkanDevice* mDevice = nullptr;

            std::shared_ptr<XJVulkanRenderPass> mRenderPass;

            std::shared_ptr<XJRenderTarget> mRenderTarget;

            VkDescriptorSet mDescriptorSet = VK_NULL_HANDLE;

            bool mOpen = true;

            bool mNeedDescriptorUpdate = true;

            bool mPendingResize = false;

            uint32_t mPendingWidth = 0;

            uint32_t mPendingHeight = 0;

            ViewportSettings mSettings;
    };
}

#endif