#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/VulkanPhysicalDevices.h"


namespace XJ
{
    XJVulkanRenderPass::XJVulkanRenderPass(XJVulkanDevice* device, VulkanPhysicalDevices* physicalDevices, const std::vector<Attachment> &attachments, const std::vector<RenderSubPass> &subPasses)
                                        : mDevice(device),mPhysicalDevices(physicalDevices), mAttachments(attachments), mSubPasses(subPasses)
    {
        // 如果附件和子通道为空，创建默认配置
        if(mSubPasses.empty())
        {
            if(mAttachments.empty())//没有附件就添加一个颜色附件    一个颜色缓冲区附件，由交换链中的一张图像表示
            {
                 // 颜色附件
                Attachment  defaultColorAttachment{};
                defaultColorAttachment.flags = 0;// 默认标志
                defaultColorAttachment.format = device->XJGetSettings().surfaceFormat;
                defaultColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
                defaultColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                defaultColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                defaultColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                defaultColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                defaultColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                defaultColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                defaultColorAttachment.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

                // 2. 深度附件 - 使用设备支持的最佳深度格式
                Attachment  defaultDepthAttachment{};
                defaultDepthAttachment.flags = 0;
                defaultDepthAttachment.format = VK_FORMAT_D32_SFLOAT; // 或者 VK_FORMAT_D24_UNORM_S8_UINT
                // 注意：深度格式将在Create方法中设置，因为我们需要先查询设备支持的格式
                defaultDepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
                defaultDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                defaultDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;// 深度附件通常不需要存储
                defaultDepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                defaultDepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                defaultDepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                defaultDepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                mAttachments.push_back(defaultColorAttachment);
                mAttachments.push_back(defaultDepthAttachment);
                spdlog::debug("添加颜色附件，格式: {}", vk_format_string(device->XJGetSettings().surfaceFormat));
            }
            RenderSubPass defaultSubPass{};
            //defaultSubPass.colorAttachments = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            //defaultSubPass.depthStencilAttachments = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}; // 添加深度附件引用
            //spdlog::debug("创建默认子通道: 颜色附件索引=0, 深度附件索引=1");
            //mSubPasses.push_back(defaultSubPass);
            defaultSubPass.colorAttachments = {0};
            defaultSubPass.depthStencilAttachments = {1}; // 添加深度附件引用
            spdlog::debug("创建默认子通道: 颜色附件索引=0, 深度附件索引=1");
            defaultSubPass.sampleCount = VK_SAMPLE_COUNT_1_BIT;  // 明确设为1
            mSubPasses.push_back(defaultSubPass);
            for (size_t i = 0; i < mAttachments.size(); ++i) 
            {
                spdlog::debug("Attachment {} samples: {}", i, mAttachments[i].samples);
            }
        }

         // 设置深度格式（查询设备支持的最佳深度格式）
        if (mAttachments.size() > 1 && mAttachments[1].format == VK_FORMAT_UNDEFINED)
        {
            // 查询最佳深度格式
            std::vector<VkFormat> depthFormats = {
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM
            };
            
            VkFormat depthFormat = VK_FORMAT_UNDEFINED;
            for (VkFormat format : depthFormats)
            {
                VkFormatProperties props;
                vkGetPhysicalDeviceFormatProperties(mPhysicalDevices->XJGetPhysicalDevice(), format, &props);
                
                if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                {
                    depthFormat = format;
                    break;
                }
            }
            
            if (depthFormat == VK_FORMAT_UNDEFINED)
            {
                spdlog::error("未找到支持的深度格式");
                throw std::runtime_error("未找到支持的深度格式");
            }
            
            mAttachments[1].format = depthFormat;
            spdlog::debug("设置深度附件格式为: {}", vk_format_string(depthFormat));
        }

    
        // 创建子通道描述
        std::vector<VkSubpassDescription> subpassDescriptions(mSubPasses.size());
        std::vector<std::vector<VkAttachmentReference>>  inputAttachmentRefs(mSubPasses.size());
        std::vector<std::vector<VkAttachmentReference>>   colorAttachmentRefs(mSubPasses.size());
        std::vector<std::vector<VkAttachmentReference>>   depthStencilAttachmentRefs(mSubPasses.size());
        VkAttachmentReference resolveAttachmentRefs[mSubPasses.size()]; // 解析附件引用数组

        for(uint32_t i = 0; i < mSubPasses.size();i++)
        {
            RenderSubPass subPass = mSubPasses[i];
            //输入通道
            // 输入附件
            for(const auto& inputAttachment : subPass.inputAttachments)
            {
                inputAttachmentRefs[i].push_back({inputAttachment, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            }
            // 颜色附件引用
            for(const auto& colorAttachment : subPass.colorAttachments)
            {
                colorAttachmentRefs[i].push_back({colorAttachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
                mAttachments[colorAttachment].samples = subPass.sampleCount; // 设置附件的采样数
                if(subPass.sampleCount > VK_SAMPLE_COUNT_1_BIT)
                {
                    mAttachments[colorAttachment].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // 多重采样附件通常不需要存储
                } else {
                    // 对于单采样，确保最终布局是PRESENT_SRC_KHR
                    mAttachments[colorAttachment].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                }
            }
            //  // 深度/模板附件引用
            for(const auto& depthStencilAttachment : subPass.depthStencilAttachments)
            {
                depthStencilAttachmentRefs[i].push_back({depthStencilAttachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
                mAttachments[depthStencilAttachment].samples = subPass.sampleCount; // 设置附件的采样数
            }
            //// 多重采样解析附件
            if(subPass.sampleCount > VK_SAMPLE_COUNT_1_BIT)
            {
                Attachment mAttachment{};
                mAttachment.format = device->XJGetSettings().surfaceFormat; // 使用与颜色附件相同的格式
                mAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;// 多重采样附件通常不需要加载
                mAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;// 多重采样附件通常需要存储以供解析使用
                mAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;// 多重采样附件通常不需要存储
                mAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;// 多重采样附件通常不需要存储
                mAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;// 解析附件通常不需要预定义布局
                mAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;// 多重采样附件通常不需要存储
                mAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // 解析附件通常不需要多重采样
                mAttachment.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                
                mAttachments.push_back(mAttachment); // 添加一个新的附件用于多重采样
                resolveAttachmentRefs[i] = {static_cast<uint32_t>(mAttachments.size() - 1), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}; // 设置解析附件引用
            }
            //subpass 描述
            subpassDescriptions[i].flags = 0;
            subpassDescriptions[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpassDescriptions[i].inputAttachmentCount = static_cast<uint32_t>(inputAttachmentRefs[i].size());
            subpassDescriptions[i].pInputAttachments = inputAttachmentRefs[i].data();
            subpassDescriptions[i].colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs[i].size());
            subpassDescriptions[i].pColorAttachments = colorAttachmentRefs[i].data();
            subpassDescriptions[i].pResolveAttachments = subPass.sampleCount > VK_SAMPLE_COUNT_1_BIT ? &resolveAttachmentRefs[i] : nullptr; // 只有在多重采样时才使用解析附件
            subpassDescriptions[i].pDepthStencilAttachment = depthStencilAttachmentRefs[i].data();
            subpassDescriptions[i].preserveAttachmentCount = 0;
            subpassDescriptions[i].pPreserveAttachments = nullptr;

        }
        //  子通道依赖关系
         // 子通道依赖关系 - 修复布局转换问题
        std::vector<VkSubpassDependency> dependencies(2);
        
        // 依赖1：从外部到子通道0 - 确保渲染开始前图像准备好
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = 0;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = 0;
        
        // 依赖2：从子通道0到外部 - 确保渲染结束后图像布局转换
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = 0;
        dependencies[1].dependencyFlags = 0;
    

        //createinfo 结构体
        std::vector<VkAttachmentDescription> vkAttachments;
        vkAttachments.reserve(mAttachments.size());
        for (const auto& attachment : mAttachments)
        {
            VkAttachmentDescription vkAttachment{};
            vkAttachment.flags = 0;
            vkAttachment.format = attachment.format;
            vkAttachment.samples = attachment.samples;
            vkAttachment.loadOp = attachment.loadOp;
            vkAttachment.storeOp = attachment.storeOp;
            vkAttachment.stencilLoadOp = attachment.stencilLoadOp;
            vkAttachment.stencilStoreOp = attachment.stencilStoreOp;
            vkAttachment.initialLayout = attachment.initialLayout;
            vkAttachment.finalLayout = attachment.finalLayout;
            vkAttachments.push_back(vkAttachment);
        }

        //创建渲染通道
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.pNext = nullptr;
        renderPassInfo.flags = 0;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(vkAttachments.size());
        renderPassInfo.pAttachments = vkAttachments.data();
        renderPassInfo.subpassCount = static_cast<uint32_t>(mSubPasses.size());
        renderPassInfo.pSubpasses = subpassDescriptions.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data(); 

        VkResult result = vkCreateRenderPass(mDevice->XJGetDevice(), &renderPassInfo, nullptr, &mRenderPass);
        if (result != VK_SUCCESS)
        {
            spdlog::error("创建渲染通道失败: {}", vk_result_string(result));
            throw std::runtime_error("创建渲染通道失败");
        }
        
        // 检查附件布局设置
        spdlog::debug("渲染通道创建成功，附件详细信息：");
        for (size_t i = 0; i < mAttachments.size(); ++i) {
            const auto& attach = mAttachments[i];
            spdlog::debug("附件[{}]: format={}, initialLayout={}, finalLayout={}, samples={}", 
                         i, vk_format_string(attach.format), 
                         attach.initialLayout, attach.finalLayout, attach.samples);
        }
        
        spdlog::trace("渲染通道{0}:{1}, 附件数量:{2}, 子通道数量:{3}, 依赖数量:{4}", 
            __FUNCTION__, (void*)mRenderPass, mAttachments.size(), mSubPasses.size(), dependencies.size());
    }
    
    XJVulkanRenderPass::~XJVulkanRenderPass()
    {
        if (mRenderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(mDevice->XJGetDevice(), mRenderPass, nullptr);
            spdlog::trace("{0} : 销毁 render pass 实例 : {1}", __FUNCTION__, (void*)mRenderPass);
            mRenderPass = VK_NULL_HANDLE;
        }
    }
    //开启渲染
    bool XJVulkanRenderPass::BeginRenderPass(VkCommandBuffer commandBuffer, XJVulkanFrameBuffer* framebuffer, const std::vector<VkClearValue>& clearValues) const
    {
        if (!framebuffer) 
        {
            spdlog::error("Null framebuffer passed to BeginRenderPass");
            return false;
        }
        if (!framebuffer->IsValid()) {
            spdlog::error("Invalid framebuffer passed to BeginRenderPass");
            return false;
        }
        VkRenderPassBeginInfo renderPassBeginInfo{};//渲染通道开始信息
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = mRenderPass;
        renderPassBeginInfo.framebuffer = framebuffer->XJGetFrameBuffer();
        renderPassBeginInfo.renderArea.offset = {0, 0};//渲染区域的偏移量
        renderPassBeginInfo.renderArea.extent = {framebuffer->XJGetWidth(), framebuffer->XJGetHeight()};//渲染区域的宽高
        renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());//清除值的数量
        renderPassBeginInfo.pClearValues = clearValues.data();//清除值

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        return true;   // 执行成功，返回 true
    }
    void XJVulkanRenderPass::EndRenderPass(VkCommandBuffer commandBuffer) const
    {
        vkCmdEndRenderPass(commandBuffer);
    }
}