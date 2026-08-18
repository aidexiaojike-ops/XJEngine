#include "Render/XJEditorFrameRenderer.h"

#include "Graphic/XJVulkanCommandBuffer.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/XJVulkanPhysicalDevices.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanSwapchain.h"
#include "Render/XJRenderContext.h"
#include "Render/XJRenderer.h"
#include "Render/XJRenderTarget.h"
#include "Render/System/XJBaseMaterialSystem.h"
#include "Render/System/XJUnlitMaterialSystem.h"

#include <spdlog/spdlog.h>

#include "Edit/XJGlfwWindow.h"
#include "Graphic/XJVulkanImage.h"
#include "UI/XJEditorRenderer.h"
#include "UI/Viewports/XJScenePreview.h"
#include "UI/Viewports/XJGamePreview.h"

#include <imgui.h>

namespace XJ
{
    XJEditorFrameRenderer::~XJEditorFrameRenderer()
    {
        Shutdown();
    }
    
    bool XJEditorFrameRenderer::Init(
        XJRenderContext& renderContext,
        VkSampleCountFlagBits sampleCount)
    {
        Shutdown();

        // 当前主 swapchain RenderPass 尚未实现多采样颜色附件与 1x resolve。
        if (sampleCount != VK_SAMPLE_COUNT_1_BIT)
        {
            spdlog::error("Editor frame renderer currently supports only VK_SAMPLE_COUNT_1_BIT.");
            return false;
        }

        if (!renderContext.XJGetDevice() ||
            !renderContext.XJGetPhysicalDevices() ||
            !renderContext.XJGetSwapchain())
        {
            spdlog::error(
                "Editor frame renderer initialization failed: "
                "Vulkan services are incomplete.");
            return false;
        }

        mRenderContext = &renderContext;
        mSampleCount = sampleCount;

        if (!CreateMainRenderTarget())
        {
            Shutdown();
            return false;
        }

        mRenderer = std::make_shared<XJRenderer>();

        if (!ReallocateCommandBuffers())
        {
            Shutdown();
            return false;
        }

        XJVulkanSwapchain* swapchain = mRenderContext->XJGetSwapchain();
        mSwapchainGeneration = swapchain->XJGetGeneration();
        mSwapchainImageCount = static_cast<uint32_t>(swapchain->XJGetSwapchainImages().size());
        mSwapchainFormat = swapchain->XJGetSurfaceInfo().surfaceFormat.format;

        mInitialized = true;
        return true;
    }

    XJRenderTarget* XJEditorFrameRenderer::GetRenderTarget() const
    {
        return mRenderTarget.get();
    }

    XJVulkanRenderPass* XJEditorFrameRenderer::GetRenderPass() const
    {
        return mRenderPass.get();
    }

    VkSampleCountFlagBits
    XJEditorFrameRenderer::GetSampleCount() const
    {
        return mSampleCount;
    }

    bool XJEditorFrameRenderer::CreateMainRenderTarget()//创建主 RenderTarget
    {
        XJVulkanDevice* device = mRenderContext->XJGetDevice();
        XJVulkanPhysicalDevices* physicalDevices =
            mRenderContext->XJGetPhysicalDevices();
        XJVulkanSwapchain* swapchain =
            mRenderContext->XJGetSwapchain();

        std::vector<Attachment> attachments(2);
        //颜色附件
        attachments[0].format =
            swapchain->XJGetSurfaceInfo().surfaceFormat.format;
        attachments[0].samples = mSampleCount;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp =
            VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp =
            VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout =
            VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout =
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        //深度附件
        attachments[1].format =
            device->XJGetSettings().depthFormat;
        attachments[1].samples = mSampleCount;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp =
            VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp =
            VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout =
            VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::vector<RenderSubPass> subpasses{
            {
                .colorAttachments = {0},
                .depthStencilAttachments = {1},
                .resolveAttachments = {},
                .sampleCount = mSampleCount
            }
        };

        mRenderPass = std::make_shared<XJVulkanRenderPass>(
            device,
            physicalDevices,
            attachments,
            subpasses);

        mRenderTarget = std::make_shared<XJRenderTarget>(
            mRenderPass.get());

        mRenderTarget->SetColorClearValue(
            0,
            VkClearColorValue{0.1f, 0.1f, 0.1f, 1.0f});

        mRenderTarget->SetDepthClearValue(
            VkClearDepthStencilValue{1.0f, 0});

        mRenderTarget->AddMaterialSystem<XJBaseMaterialSystem>();
        mRenderTarget->AddMaterialSystem<XJUnlitMaterialSystem>();

        return true;
    }

    bool XJEditorFrameRenderer::ReallocateCommandBuffers()
    {
        if (!mRenderContext)
            return false;

        XJVulkanDevice* device = mRenderContext->XJGetDevice();

        XJVulkanSwapchain* swapchain = mRenderContext->XJGetSwapchain();

        if (!device || !swapchain)
            return false;

        auto* commandPool =
            device->XJGetDefaultCmdPool();

        if (!commandPool)
            return false;

        const uint32_t count = static_cast<uint32_t>(swapchain->XJGetSwapchainImages().size());

        if (count == 0)
        {
            spdlog::error(
                "Cannot allocate command buffers: "
                "swapchain image count is zero.");
            return false;
        }

        // Swapchain image 数量没有变化时，已有 command buffer 可以继续复用。
        // 初次初始化时 vector 为空，因此不会从这里提前返回。
        if (mCommandBuffers.size() == count)
            return true;

        device->WaitIdle();

        for (VkCommandBuffer commandBuffer : mCommandBuffers)
        {
            if (commandBuffer != VK_NULL_HANDLE)
                commandPool->FreeCommandBuffer(commandBuffer);
        }

        mCommandBuffers.clear();

        mCommandBuffers = commandPool->AllocateCommandBuffer(count);

        if (mCommandBuffers.size() != count)
        {
            spdlog::error(
                "Command buffer allocation failed: "
                "expected={}, actual={}",
                count,
                mCommandBuffers.size());

            // AllocateCommandBuffer 可能返回部分结果，clear 前必须逐个归还。
            for (VkCommandBuffer commandBuffer : mCommandBuffers)
            {
                if (commandBuffer != VK_NULL_HANDLE)
                    commandPool->FreeCommandBuffer(commandBuffer);
            }
            mCommandBuffers.clear();
            return false;
        }

        return true;
    }

    void XJEditorFrameRenderer::Shutdown()
    {
        if (!mRenderContext)
            return;

        XJVulkanDevice* device = mRenderContext->XJGetDevice();
        if (device)
        {
            device->WaitIdle();

            if (auto* pool = device->XJGetDefaultCmdPool())
            {
                for (VkCommandBuffer commandBuffer : mCommandBuffers)
                    pool->FreeCommandBuffer(commandBuffer);
            }
        }

        mCommandBuffers.clear();
        mRenderer.reset();

        // RenderTarget 必须先于 RenderPass 释放。
        mRenderTarget.reset();
        mRenderPass.reset();

        mRenderContext = nullptr;
        mSwapchainGeneration = 0;
        mSwapchainImageCount = 0;
        mSwapchainFormat = VK_FORMAT_UNDEFINED;
        mInitialized = false;
    }

    void XJEditorFrameRenderer::UpdatePlatformWindows()//多窗口更新
    {
        if (!ImGui::GetCurrentContext())
            return;

        if (!(ImGui::GetIO().ConfigFlags &
              ImGuiConfigFlags_ViewportsEnable))
        {
            return;
        }

        ImGui::UpdatePlatformWindows();

        // Vulkan 平台窗口可能使用自己的 swapchain。
        // 必须在主 command buffer 提交后调用。
        ImGui::RenderPlatformWindowsDefault();
    }

    bool XJEditorFrameRenderer::ResizeSwapchainResources()//设置交换链大小
    {
        if (!mRenderContext)
            return false;

        XJVulkanSwapchain* swapchain =
            mRenderContext->XJGetSwapchain();

        if (!swapchain)
            return false;

        const VkFormat currentFormat = swapchain->XJGetSurfaceInfo().surfaceFormat.format;
        if (currentFormat != mSwapchainFormat)
        {
            spdlog::critical("Swapchain format changed; editor RenderPass/UI backend recreation is required.");
            return false;
        }

        const uint64_t generation = swapchain->XJGetGeneration();
        const uint32_t imageCount = static_cast<uint32_t>(swapchain->XJGetSwapchainImages().size());
        const bool generationChanged = generation != mSwapchainGeneration;

        if (mRenderTarget)
        {
            mRenderTarget->SetExtent({
                swapchain->XJGetWidth(),
                swapchain->XJGetHeight()
            });

            mRenderTarget->SetBufferCount(imageCount);

            if (generationChanged)
                mRenderTarget->RequestRecreate();

            // SetExtent 只设置 dirty 标记。必须在 command buffer
            // 录制之外重建 framebuffer。
            mRenderTarget->UpdateIfNeeded();
        }

        if (!ReallocateCommandBuffers())
        {
            spdlog::error(
                "Failed to resize editor command buffers.");
            return false;
        }

        if (mSwapchainImageCount != 0 && imageCount != mSwapchainImageCount)
        {
            spdlog::warn(
                "Swapchain image count changed from {} to {}; ImGui Vulkan backend was initialized with the old count.",
                mSwapchainImageCount,
                imageCount);
        }

        mSwapchainGeneration = generation;
        mSwapchainImageCount = imageCount;
        return true;
    }


    bool XJEditorFrameRenderer::RenderFrame(const XJEditorFrameRenderInput& input)//渲染帧
    {
        if (!mInitialized || !mRenderContext || !mRenderer || !mRenderTarget || !input.Window)
        {
            return false;
        }

        if (input.Window->IsWindowMinimized())
        {
            UpdatePlatformWindows();
            return true;
        }

        XJVulkanDevice* device = mRenderContext->XJGetDevice();
        XJVulkanSwapchain* swapchain = mRenderContext->XJGetSwapchain();

        if (!device || !swapchain)
            return false;

        if (swapchain->XJGetGeneration() != mSwapchainGeneration)
        {
            if (!ResizeSwapchainResources())
                return false;
        }

        if (input.UIRenderer)
            input.UIRenderer->UpdateSwapchainImageCount(mSwapchainImageCount);

        XJFrameAcquireResult acquire = mRenderer->XJRendererBegin(mCommandBuffers);

        if (acquire.resizeNeeded && !ResizeSwapchainResources())
            return false;

        if (!acquire.acquired)
        {
            UpdatePlatformWindows();
            return false;
        }

        if (input.UIRenderer)
            input.UIRenderer->UpdateSwapchainImageCount(mSwapchainImageCount);

        if (acquire.imageIndex < 0 ||
            static_cast<size_t>(acquire.imageIndex) >=
                mCommandBuffers.size())
        {
            spdlog::error("Invalid swapchain image index: {}", acquire.imageIndex);
            return false;
        }

        // 离屏 RenderTarget 的 resize 必须在开始录制前处理。
        if (input.ScenePreview)
            input.ScenePreview->PrepareBeforeRender();

        if (input.GamePreview)
            input.GamePreview->PrepareBeforeRender();

        VkCommandBuffer commandBuffer = mCommandBuffers[acquire.imageIndex];

        if (commandBuffer == VK_NULL_HANDLE)
            return false;

        XJVulkanCommandPool::BeginCommandBuffer(commandBuffer);

        // 先渲染离屏 Scene/Game viewport。
        if (input.ScenePreview)
            input.ScenePreview->Render(commandBuffer);

        if (input.GamePreview)
            input.GamePreview->Render(commandBuffer);

        if (input.ScenePreview)
            input.ScenePreview->PostRender();

        if (input.GamePreview)
            input.GamePreview->PostRender();

        const bool mainPassBegan = mRenderTarget->BeginRenderTarget(commandBuffer);

        if (mainPassBegan)
        {
            if (input.UIRenderer && input.DrawData)
            {
                input.UIRenderer->RenderDrawData(
                    commandBuffer,
                    input.DrawData);
            }

            mRenderTarget->EndRenderTarget(commandBuffer);
        }
        else
        {
            // 即使主 render pass 未开始，也要把 swapchain image
            // 转换到 present layout，避免提交无效布局。
            const auto& images =
                swapchain->XJGetSwapchainImages();

            if (static_cast<size_t>(acquire.imageIndex) <
                images.size())
            {
                XJVulkanImage::TransitionLayout(
                    commandBuffer,
                    images[acquire.imageIndex],
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            }
        }

        XJVulkanCommandPool::EndCommandBuffer(commandBuffer);

        XJFramePresentResult present =
            mRenderer->XJRendererEnd(
                acquire.imageIndex,
                {commandBuffer});

        // 主 command buffer 已提交，现在才允许渲染平台窗口。
        UpdatePlatformWindows();

        if (present.resizeNeeded)
        {
            if (!ResizeSwapchainResources())
                return false;

            if (input.UIRenderer)
                input.UIRenderer->UpdateSwapchainImageCount(mSwapchainImageCount);
        }

        return present.presented;
    }
}
