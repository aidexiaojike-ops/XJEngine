#ifndef XJ_VULKAN_RENDERPASS_H
#define XJ_VULKAN_RENDERPASS_H

#include "Graphic/VulkanCommon.h"
//m = member（成员变量）
namespace XJ
{
    class XJVulkanDevice;
    class XJVulkanFrameBuffer;
    class VulkanPhysicalDevices;

    struct Attachment  //附件 在窗户或者画面能直接看到的效果 或者深度附件 模板 橡树等等
    {
        VkAttachmentDescriptionFlags    flags;
        VkFormat                        format = VK_FORMAT_UNDEFINED;
        VkAttachmentLoadOp              loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp             storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkAttachmentLoadOp              stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp             stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkImageLayout                   initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout                   finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkSampleCountFlagBits           samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageUsageFlags               usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 新增：图像使用标志
    };
    
    struct RenderSubPass//子流程
    {
        std::vector<uint32_t>  inputAttachments;//输入附件
        std::vector<uint32_t>  colorAttachments;//颜色附加
        std::vector<uint32_t>  depthStencilAttachments;//深度附加
        std::vector<uint32_t> resolveAttachments; // 新增：解析附件索引
        VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;//多重附加 采样倍数
    };
  
    class XJVulkanRenderPass
    {
        private:
            XJVulkanDevice *mDevice;
            VulkanPhysicalDevices* mPhysicalDevices;
            VkRenderPass mRenderPass = VK_NULL_HANDLE;

         
            std::vector<RenderSubPass> mSubPasses;
            std::vector<Attachment> mAttachments;

            /* data */
        public:
            XJVulkanRenderPass(XJVulkanDevice* device, VulkanPhysicalDevices* physicalDevices, const std::vector<Attachment> &attachments = {}, const std::vector<RenderSubPass> &subPass = {});
            ~XJVulkanRenderPass();

            VkRenderPass XJGetRenderPass() const{return mRenderPass;}

            const std::vector<Attachment> &XJGetAttachments() const { return mAttachments; }
            uint32_t XJGetAttachmentSize() const { return static_cast<uint32_t>(mAttachments.size()); }
            const std::vector<RenderSubPass> &XJGetSubPasses() const { return mSubPasses; }

            uint32_t XJGetColorAttachmentCount(uint32_t subpassIndex = 0) const
            {
                if (subpassIndex >= mSubPasses.size())
                    return 0;
            
                const auto& subpass = mSubPasses[subpassIndex];
                return  static_cast<uint32_t>(subpass.colorAttachments.size());
            }

            bool BeginRenderPass(VkCommandBuffer commandBuffer, XJVulkanFrameBuffer* framebuffer, const std::vector<VkClearValue>& clearValues) const;
            void EndRenderPass(VkCommandBuffer commandBuffer) const;
    };
    

    
}

#endif