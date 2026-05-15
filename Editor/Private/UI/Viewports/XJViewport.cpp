#include "UI/Viewports/XJViewport.h"

#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanDevice.h"
#include "Render/XJRenderTarget.h"

#include "Render/XJRenderContext.h"

#include "imgui_impl_vulkan.h"

namespace XJ
{
    bool XJViewport::Init(XJRenderContext* renderContext)
    {
        if (!renderContext)
            return false;

        mRenderContext = renderContext;

        mDevice = renderContext->XJGetDevice();

        if (!mDevice)
            return false;

        VulkanPhysicalDevices* physicalDevices =
            renderContext->XJGetPhysicalDevices();

        if (!physicalDevices)
            return false;
        
        CreateRenderPass(physicalDevices);

        VkExtent2D extent
        {
            mSettings.mWidth,
            mSettings.mHeight
        };

        mRenderTarget =
            std::make_shared<XJRenderTarget>(
                mRenderPass.get(),
                1,
                extent
            );

        mNeedDescriptorUpdate = true;

        return true;
    }
    void XJViewport::Resize(uint32_t width, uint32_t height)
    {
        if (width < 64 || height < 64)
            return;

        if (width == mSettings.mWidth && height == mSettings.mHeight)
            return;

        mPendingResize = true;
        mPendingWidth = width;
        mPendingHeight = height;
    }

    void XJViewport::RecreateDescriptor()
    {
        if (!mRenderTarget)
            return;

            /*如果 mRenderTarget 当前 buffer index 还没经过一次 BeginRenderTarget() 更新，
            可能拿到的不是刚重建后你想展示的那张。因为 preview render target 是 bufferCount = 1，
            目前问题不大。但如果以后 preview 多 buffer，这里要改成明确取当前 preview 的显示 framebuffer。
            当前你可以先不改，scene preview 是 1 buffer，够用。*/
        XJVulkanFrameBuffer* frameBuffer = mRenderTarget->XJGetCurrentFrameBuffer();
        if (!frameBuffer)
            return;

        VkImageView view = frameBuffer->XJGetColorImageViewHandle(0);
        if (view == VK_NULL_HANDLE)
            return;

        ReleaseDescriptor();
        //spdlog::warn("RecreateDescriptor: AddTexture, view={}, oldDesc={}", (void*)view, (void*)mDescriptorSet);
        mDescriptorSet = ImGui_ImplVulkan_AddTexture(
            view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        //spdlog::warn("RecreateDescriptor: AddTexture done, newDesc={}", (void*)mDescriptorSet);
        mNeedDescriptorUpdate = false;
    }

    void XJViewport::ReleaseDescriptor()
    {
        if (mDescriptorSet != VK_NULL_HANDLE)
        {
            VkDescriptorSet oldSet = mDescriptorSet;
            ImGui_ImplVulkan_RemoveTexture(mDescriptorSet);
            mDescriptorSet = VK_NULL_HANDLE;
            spdlog::trace("Release viewport descriptor safely: desc={}", (void*)oldSet);
        }

    }


    void XJViewport::DrawUI()
    {
        if (!mOpen)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        if (ImGui::Begin(mSettings.mViewportName, &mOpen))
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            
            if (avail.x > 1.0f && avail.y > 1.0f)
            {
                Resize(static_cast<uint32_t>(avail.x), static_cast<uint32_t>(avail.y));

                if (mPendingResize)
                {
                    mNeedDescriptorUpdate = true;
                }

                if (!mPendingResize && mDescriptorSet != VK_NULL_HANDLE)
                {
                    //spdlog::warn("DrawUI Image: desc={} avail=({},{})", (void*)mDescriptorSet, avail.x, avail.y);
                    ImGui::Image((ImTextureID)mDescriptorSet, avail);
                }
                else
                {
                    ImGui::TextUnformatted("Scene Preview Texture Invalid.");
                }
            }
            else
            {
                ImGui::TextUnformatted("Preview window too small.");
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void XJViewport::Shutdown()
    {
        if (mDevice)
        {
            vkDeviceWaitIdle(mDevice->XJGetDevice());
        }

        ReleaseDescriptor();

        mRenderTarget.reset();
        mRenderPass.reset();
    }
    bool XJViewport::BeginViewportRender(VkCommandBuffer cmd)
    {
        if (!mRenderTarget || !cmd)
            return false;

        return mRenderTarget->BeginRenderTarget(cmd);
    }

    void XJViewport::PrepareBeforeRender()
    {
        if (!mRenderTarget || !mDevice)
            return; 

        if (mPendingResize)
        {
            ReleaseDescriptor();    

            mSettings.mWidth = mPendingWidth;
            mSettings.mHeight = mPendingHeight; 

            spdlog::warn("Viewport apply resize safely: {}x{}", mPendingWidth, mPendingHeight);
            VkExtent2D extent
            {
                mSettings.mWidth,
                mSettings.mHeight
            };  

            mRenderTarget->SetExtent(extent);
            mRenderTarget->UpdateIfNeeded();

            mNeedDescriptorUpdate = true;
            mPendingResize = false;
        }

    }
    void XJViewport::PostRender()
    {
        if (mNeedDescriptorUpdate)
        {
            RecreateDescriptor();
        }
    }
    void XJViewport::EndViewportRender(VkCommandBuffer cmd)
    {
        mRenderTarget->EndRenderTarget(cmd);

    }

    void XJViewport::CreateRenderPass(VulkanPhysicalDevices* physicalDevices)
    {
        VkFormat colorFormat =
            mDevice->XJGetSettings().surfaceFormat;

        Attachment colorAttachment{};
        colorAttachment.format = colorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        colorAttachment.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        std::vector<Attachment> attachments =
        {
            colorAttachment
        };

        std::vector<RenderSubPass> subpasses =
        {
            {
                .colorAttachments = {0},
                .depthStencilAttachments = {},
                .resolveAttachments = {},
                .sampleCount = VK_SAMPLE_COUNT_1_BIT
            }
        };

        mRenderPass =
            std::make_shared<XJVulkanRenderPass>(
                mDevice,
                physicalDevices,
                attachments,
                subpasses
            );
    }
    
}