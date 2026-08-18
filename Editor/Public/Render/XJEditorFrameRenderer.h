#ifndef XJ_EDITOR_FRAME_RENDERER_H
#define XJ_EDITOR_FRAME_RENDERER_H

#include "Graphic/VulkanCommon.h"

#include <memory>
#include <vector>

struct ImDrawData;

namespace XJ
{
    class XJGlfwWindow;
    class XJRenderContext;
    class XJRenderTarget;
    class XJRenderer;
    class XJVulkanRenderPass;
    class XJVulkanDevice;
    class XJVulkanSwapchain;
    class XJEditorRenderer;
    class XJScenePreview;
    class XJGamePreview;

    struct XJEditorFrameRenderInput
    {
        XJGlfwWindow* Window = nullptr;
        XJEditorRenderer* UIRenderer = nullptr;
        XJScenePreview* ScenePreview = nullptr;
        XJGamePreview* GamePreview = nullptr;
        ImDrawData* DrawData = nullptr;
    };

    class XJEditorFrameRenderer
    {
        public:
            XJEditorFrameRenderer() = default;
            ~XJEditorFrameRenderer();
        
            XJEditorFrameRenderer(const XJEditorFrameRenderer&) = delete;
            XJEditorFrameRenderer& operator=(const XJEditorFrameRenderer&) = delete;
        
            bool Init(
                XJRenderContext& renderContext,
                VkSampleCountFlagBits sampleCount);
            
            bool RenderFrame(const XJEditorFrameRenderInput& input);
            void Shutdown();
            
            XJRenderTarget* GetRenderTarget() const;
            XJVulkanRenderPass* GetRenderPass() const;
            VkSampleCountFlagBits GetSampleCount() const;
            
        private:
            bool CreateMainRenderTarget();
            bool ReallocateCommandBuffers();
            bool ResizeSwapchainResources();
            void UpdatePlatformWindows();
            
            XJRenderContext* mRenderContext = nullptr;
            
            // RenderTarget 保存 RenderPass 裸指针，声明顺序不能交换。
            std::shared_ptr<XJVulkanRenderPass> mRenderPass;
            std::shared_ptr<XJRenderTarget> mRenderTarget;
            std::shared_ptr<XJRenderer> mRenderer;
            
            std::vector<VkCommandBuffer> mCommandBuffers;
            
            VkSampleCountFlagBits mSampleCount =
                VK_SAMPLE_COUNT_1_BIT;
            uint64_t mSwapchainGeneration = 0;
            uint32_t mSwapchainImageCount = 0;
            VkFormat mSwapchainFormat = VK_FORMAT_UNDEFINED;
            
            bool mInitialized = false;
    };
}

#endif
