#include "UI/Viewports/XJGamePreview.h"
// #include "imgui_impl_vulkan.h"
#include "Render/XJRenderTarget.h"
#include "ECS/XJEntity.h"
#include "Graphic/XJVulkanRenderPass.h"   
#include "Graphic/XJVulkanDevice.h"   

namespace XJ
{
    bool XJGamePreview::Render(VkCommandBuffer cmd)
    {
        // ★ 先设摄像机，再 Begin（Begin 需要用它更新 aspect ratio）
        if (mGameCamera)
            mRenderTarget->XJSetCamera(mGameCamera);
        if (!BeginViewportRender(cmd))
            return false;


        mRenderTarget->RenderMaterialSystem(cmd);
        EndViewportRender(cmd);
        return true;
    }
    
    void XJGamePreview::CreateRenderPass(VulkanPhysicalDevices* physicalDevices)
    {
        VkFormat colorFormat = mDevice->XJGetSettings().surfaceFormat;
        VkFormat depthFormat = mDevice->XJGetSettings().depthFormat;

        Attachment colorAttachment{};
        colorAttachment.format = colorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        colorAttachment.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        Attachment depthAttachment{};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        std::vector<Attachment> attachments =
        {
            colorAttachment,
            depthAttachment
        };

        std::vector<RenderSubPass> subpasses =
        {
            {
                .colorAttachments = { 0 },
                .depthStencilAttachments = { 1 },
                .resolveAttachments = {},
                .sampleCount = VK_SAMPLE_COUNT_1_BIT
            }
        };

        mRenderPass = std::make_shared<XJVulkanRenderPass>(
            mDevice,
            physicalDevices,
            attachments,
            subpasses
        );
    }

}