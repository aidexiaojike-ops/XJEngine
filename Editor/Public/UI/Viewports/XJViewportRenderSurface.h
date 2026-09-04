#ifndef XJ_VIEWPORT_RENDER_SURFACE_H
#define XJ_VIEWPORT_RENDER_SURFACE_H

#include "Graphic/VulkanCommon.h"
#include "Render/XJRenderTarget.h"

#include <imgui.h>
#include <memory>
#include <utility>
#include <vector>

namespace XJ
{
    class XJEntity;
    class XJRenderContext;
    class XJVulkanDevice;
    class XJVulkanPhysicalDevices;
    class XJVulkanRenderPass;
    class XJScene;

    class XJViewportRenderSurface
    {
        public:
            ~XJViewportRenderSurface();

            bool Init(XJRenderContext* renderContext, uint32_t width, uint32_t height, bool useDepth);
            void Shutdown();

            void Resize(uint32_t width, uint32_t height);
            void PrepareBeforeRender();
            void PostRender();

            bool BeginRender(VkCommandBuffer cmd);
            void EndRender(VkCommandBuffer cmd);
            void RenderMaterialSystem(VkCommandBuffer cmd);
            void SetCamera(XJEntity* camera);
            void SetScene(XJScene* scene);

            ImTextureID GetTextureID() const { return reinterpret_cast<ImTextureID>(mDescriptorSet); }
            bool IsTextureReady() const { return !mPendingResize && mDescriptorSet != VK_NULL_HANDLE; }
            XJRenderTarget* GetRenderTarget() const { return mRenderTarget.get(); }

            template<typename T, typename... Args>
            void AddMaterialSystem(Args&&... args)
            {
                if (mRenderTarget)
                    mRenderTarget->template AddMaterialSystem<T>(std::forward<Args>(args)...);
            }

        private:
            struct PendingDescriptorRelease
            {
                VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
                uint32_t FramesLeft = 0;
            };

            void CreateRenderPass(XJVulkanPhysicalDevices* physicalDevices);
            void RecreateDescriptor();
            void ReleaseDescriptor();
            void QueueDescriptorRelease(VkDescriptorSet descriptorSet);
            void ProcessPendingDescriptorReleases();
            void FlushPendingDescriptorReleases();

            XJRenderContext* mRenderContext = nullptr;
            XJVulkanDevice* mDevice = nullptr;

            std::shared_ptr<XJVulkanRenderPass> mRenderPass;
            std::shared_ptr<XJRenderTarget> mRenderTarget;

            std::vector<PendingDescriptorRelease> mPendingDescriptorReleases;

            VkDescriptorSet mDescriptorSet = VK_NULL_HANDLE;

            bool mUseDepth = false;
            bool mNeedDescriptorUpdate = true;
            bool mPendingResize = false;

            uint32_t mWidth = 64;
            uint32_t mHeight = 64;
            uint32_t mPendingWidth = 0;
            uint32_t mPendingHeight = 0;
    };
}

#endif
