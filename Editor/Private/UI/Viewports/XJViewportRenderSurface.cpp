#include "UI/Viewports/XJViewportRenderSurface.h"

#include "Graphic/XJVulkanDevice.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Render/XJRenderContext.h"
#include "Render/XJRenderer.h"

#include "imgui_impl_vulkan.h"
#include <spdlog/spdlog.h>

namespace XJ
{
    XJViewportRenderSurface::~XJViewportRenderSurface()
    {
        Shutdown();
    }

    bool XJViewportRenderSurface::Init(XJRenderContext* renderContext, uint32_t width, uint32_t height, bool useDepth)
    {
        Shutdown();

        if (!renderContext)
            return false;

        mRenderContext = renderContext;
        mDevice = renderContext->XJGetDevice();
        mUseDepth = useDepth;
        mWidth = width;
        mHeight = height;

        if (!mDevice)
            return false;

        XJVulkanPhysicalDevices* physicalDevices = renderContext->XJGetPhysicalDevices();
        if (!physicalDevices)
            return false;

        CreateRenderPass(physicalDevices);
        if (!mRenderPass)
            return false;

        VkExtent2D extent{mWidth, mHeight};
        mRenderTarget = std::make_shared<XJRenderTarget>(mRenderPass.get(), 1, extent);
        mNeedDescriptorUpdate = true;
        return true;
    }

    void XJViewportRenderSurface::Shutdown()
    {
        if (mDevice)
            vkDeviceWaitIdle(mDevice->XJGetDevice());

        ReleaseDescriptor();
        FlushPendingDescriptorReleases();

        mRenderTarget.reset();
        mRenderPass.reset();
        mRenderContext = nullptr;
        mDevice = nullptr;
        mNeedDescriptorUpdate = false;
        mPendingResize = false;
    }

    void XJViewportRenderSurface::Resize(uint32_t width, uint32_t height)
    {
        if (width < 64 || height < 64)
            return;

        if (width == mWidth && height == mHeight)
            return;

        mPendingResize = true;
        mPendingWidth = width;
        mPendingHeight = height;
    }

    void XJViewportRenderSurface::PrepareBeforeRender()
    {
        ProcessPendingDescriptorReleases();

        if (!mRenderTarget || !mDevice)
            return;

        if (!mPendingResize)
            return;

        ReleaseDescriptor();

        mWidth = mPendingWidth;
        mHeight = mPendingHeight;

        spdlog::warn("Viewport apply resize safely: {}x{}", mPendingWidth, mPendingHeight);

        VkExtent2D extent{mWidth, mHeight};
        mRenderTarget->SetExtent(extent);
        mRenderTarget->UpdateIfNeeded();

        mNeedDescriptorUpdate = true;
        mPendingResize = false;
    }

    void XJViewportRenderSurface::PostRender()
    {
        if (mNeedDescriptorUpdate)
            RecreateDescriptor();
    }

    bool XJViewportRenderSurface::BeginRender(VkCommandBuffer cmd)
    {
        if (!mRenderTarget || !cmd)
            return false;

        return mRenderTarget->BeginRenderTarget(cmd);
    }

    void XJViewportRenderSurface::EndRender(VkCommandBuffer cmd)
    {
        if (mRenderTarget)
            mRenderTarget->EndRenderTarget(cmd);
    }

    void XJViewportRenderSurface::RenderMaterialSystem(VkCommandBuffer cmd)
    {
        if (mRenderTarget)
            mRenderTarget->RenderMaterialSystem(cmd);
    }

    void XJViewportRenderSurface::SetCamera(XJEntity* camera)
    {
        if (!mRenderTarget)
            return;

        if (camera)
        {
            // RenderTarget 只保存相机 UUID，不长期持有 XJEntity 裸指针。
            mRenderTarget->XJSetCamera(camera);
        }
        else
        {
            // 相机被删除或场景卸载时必须主动清空旧 UUID，
            // 不能因为 camera == nullptr 就跳过设置。
            mRenderTarget->XJClearCamera();
        }
    }

    void XJViewportRenderSurface::SetScene(XJScene* scene)
    {
        if(mRenderTarget)
        {
            mRenderTarget->SetScene(scene);
        }
    }

    void XJViewportRenderSurface::CreateRenderPass(XJVulkanPhysicalDevices* physicalDevices)
    {
        VkFormat colorFormat = mDevice->XJGetSettings().surfaceFormat;

        Attachment colorAttachment{};
        colorAttachment.format = colorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        colorAttachment.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        std::vector<Attachment> attachments{colorAttachment};
        std::vector<uint32_t> depthAttachments;

        if (mUseDepth)
        {
            Attachment depthAttachment{};
            depthAttachment.format = mDevice->XJGetSettings().depthFormat;
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

            attachments.push_back(depthAttachment);
            depthAttachments.push_back(1);
        }

        std::vector<RenderSubPass> subpasses =
        {
            {
                .colorAttachments = {0},
                .depthStencilAttachments = depthAttachments,
                .resolveAttachments = {},
                .sampleCount = VK_SAMPLE_COUNT_1_BIT
            }
        };

        mRenderPass = std::make_shared<XJVulkanRenderPass>(mDevice, physicalDevices, attachments, subpasses);
    }

    void XJViewportRenderSurface::RecreateDescriptor()
    {
        if (!mRenderTarget)
            return;

        XJVulkanFrameBuffer* frameBuffer = mRenderTarget->XJGetCurrentFrameBuffer();
        if (!frameBuffer)
            return;

        VkImageView view = frameBuffer->XJGetColorImageViewHandle(0);
        if (view == VK_NULL_HANDLE)
            return;

        ReleaseDescriptor();
        mDescriptorSet = ImGui_ImplVulkan_AddTexture(view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        mNeedDescriptorUpdate = false;
    }

    void XJViewportRenderSurface::ReleaseDescriptor()
    {
        if (mDescriptorSet == VK_NULL_HANDLE)
            return;

        QueueDescriptorRelease(mDescriptorSet);
        mDescriptorSet = VK_NULL_HANDLE;
    }

    void XJViewportRenderSurface::QueueDescriptorRelease(VkDescriptorSet descriptorSet)
    {
        if (descriptorSet == VK_NULL_HANDLE)
            return;

        // Submitted ImGui draw commands may still reference this descriptor.
        // Keep it alive until every in-flight frame slot has had a chance to retire.
        mPendingDescriptorReleases.push_back({descriptorSet, RENDERER_NUM_BUFFER + 1});
    }

    void XJViewportRenderSurface::ProcessPendingDescriptorReleases()
    {
        for (auto it = mPendingDescriptorReleases.begin(); it != mPendingDescriptorReleases.end();)
        {
            if (it->FramesLeft > 0)
                --it->FramesLeft;

            if (it->FramesLeft == 0)
            {
                ImGui_ImplVulkan_RemoveTexture(it->DescriptorSet);
                spdlog::trace("Release delayed viewport descriptor: desc={}", (void*)it->DescriptorSet);
                it = mPendingDescriptorReleases.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void XJViewportRenderSurface::FlushPendingDescriptorReleases()
    {
        for (const auto& item : mPendingDescriptorReleases)
        {
            if (item.DescriptorSet != VK_NULL_HANDLE)
                ImGui_ImplVulkan_RemoveTexture(item.DescriptorSet);
        }

        mPendingDescriptorReleases.clear();
    }
}
