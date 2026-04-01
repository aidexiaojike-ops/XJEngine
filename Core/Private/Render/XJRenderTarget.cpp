#include "Render/XJRenderTarget.h"
#include "XJApplication.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanImage.h"
#include "spdlog/spdlog.h"
#include "Graphic/XJVulkanDepthImage.h"

namespace XJ
{
    XJRenderTarget::XJRenderTarget(XJVulkanRenderPass *rederPass)
    {
        XJRenderContext *renderContext = XJApplication::XJGetAppContext()->renderContext;
        if (!renderContext)
        {
            spdlog::error("Render context is null in XJRenderTarget constructor");
            return;
        }
        XJVulkanSwapchain* swapchain = renderContext->XJGetSwapchain();
        if (!swapchain)
        {
            spdlog::error("Swapchain is null in XJRenderTarget constructor");
            return;
        }

        mRenderPass = rederPass;
        mBufferCount = swapchain->XJGetSwapchainImages().size();
        mExtent = {swapchain->XJGetWidth(), swapchain->XJGetHeight()};
        bSwapchainTarget = true;

        Init();
        ReCreate();
        // 如果创建失败，至少设置一个默认帧缓冲区
        if (mFrameBuffers.empty()) 
        {
            spdlog::error("Failed to create any framebuffers in constructor");
            mBufferCount = std::max(1u, mBufferCount);
        }
        
    }
    XJRenderTarget::XJRenderTarget(XJVulkanRenderPass *rederPass,uint32_t bufferCount, VkExtent2D extent)
                :   mRenderPass(rederPass), mBufferCount(bufferCount), mExtent(extent), bSwapchainTarget(false)
    {
        Init();
        ReCreate();
    }
    XJRenderTarget::~XJRenderTarget()
    {
        
    }
    void XJRenderTarget::Init()
    {
        mClearValues.resize(mRenderPass->XJGetAttachmentSize());
        SetColorClearValue({0.0f, 0.0f, 0.0f, 1.0f});//默认颜色清除值为黑色
        SetDepthClearValue({1.0f, 0});//默认深度清除值为1.0，模板清除值为0
        
    }
    void XJRenderTarget::ReCreate()
    {
        // 备份原有帧缓冲区
        auto oldFrameBuffers = std::move(mFrameBuffers);
        mFrameBuffers.clear();

        // 检查渲染通道是否有效
        if (!mRenderPass) {
            spdlog::error("Render pass is null in ReCreate");
            return;
        }

        if(mExtent.width == 0 || mExtent.height == 0 || mBufferCount == 0)
        {
            spdlog::error("Invalid render target extent or buffer count. ReCreate aborted.");
            return;
        }

        if(mExtent.width == 0 || mExtent.height == 0 || mBufferCount == 0)
        {
            spdlog::error("Invalid render target extent or buffer count. ReCreate aborted.");
            return;
        }
        mFrameBuffers.clear();
        mDepthImages.clear();
        mFrameBuffers.reserve(mBufferCount);

        XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();
        XJVulkanSwapchain* kSwapchain = kRenderContext->XJGetSwapchain();

        std::vector<Attachment> kAttachments = mRenderPass->XJGetAttachments();
        if(kAttachments.empty())
        {
            spdlog::error("Render pass has no attachments. ReCreate aborted.");
            return;
        }
        // 改为：分析所有附件
        std::vector<int> colorAttachmentIndices;
        std::vector<int> depthAttachmentIndices;
        std::vector<int> resolveAttachmentIndices;

        // 先检查子通道的解析附件引用
        std::vector<bool> isResolveAttachment(kAttachments.size(), false);
        const auto& subPasses = mRenderPass->XJGetSubPasses();
        if (!subPasses.empty()) 
        {
            const auto& firstSubPass = subPasses[0];
            for (uint32_t resolveIdx : firstSubPass.resolveAttachments) {
                if (resolveIdx < kAttachments.size()) {
                    isResolveAttachment[resolveIdx] = true;
                    spdlog::debug("标记为解析附件: 索引{}", resolveIdx);
                }
            }
            
            // 如果没有明确标记，根据特征推断：单采样、颜色格式、最终布局为PRESENT_SRC_KHR
            if (firstSubPass.resolveAttachments.empty() && firstSubPass.sampleCount > VK_SAMPLE_COUNT_1_BIT) {
                for (size_t idx = 0; idx < kAttachments.size(); idx++) {
                    const auto& attach = kAttachments[idx];
                    if (!IsDepthStencilFormat(attach.format) && 
                        attach.samples == VK_SAMPLE_COUNT_1_BIT &&
                        attach.finalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                        isResolveAttachment[idx] = true;
                        spdlog::debug("推断为解析附件: 索引{}", idx);
                    }
                }
            }
        }

        for (size_t idx = 0; idx < kAttachments.size(); idx++) 
        {
            const auto& attach = kAttachments[idx];
            if (IsDepthStencilFormat(attach.format)) 
            {
                depthAttachmentIndices.push_back(idx);
                spdlog::debug("深度附件[{}]: format={}", idx, vk_format_string(attach.format));
            } 
            else if (isResolveAttachment[idx])
            {
                resolveAttachmentIndices.push_back(idx);
                spdlog::debug("解析附件[{}]: format={}, samples={}", 
                             idx, vk_format_string(attach.format), attach.samples);
            }
            else 
            {
                colorAttachmentIndices.push_back(idx);
                spdlog::debug("颜色附件[{}]: format={}, samples={}", 
                             idx, vk_format_string(attach.format), attach.samples);
            }
        }

        spdlog::debug("附件分析：总数={}, 颜色={}, 深度={}, 解析={}",
                      kAttachments.size(), colorAttachmentIndices.size(),
                      depthAttachmentIndices.size(), resolveAttachmentIndices.size());
        for (int idx : colorAttachmentIndices) 
        {
            const auto& attach = kAttachments[idx];
            spdlog::debug("颜色附件[{}]: format={}, finalLayout={}, samples={}", 
                          idx, vk_format_string(attach.format), 
                          attach.finalLayout, attach.samples);
        }
        
        // 验证附件数量与渲染通道匹配
        if (colorAttachmentIndices.empty()) {
            spdlog::error("没有找到颜色附件");
            return;
        }

        std::vector<VkImage> kSwapchainImages = kSwapchain->XJGetSwapchainImages();
        for(int i = 0; i < mBufferCount; i++)
        {
            std::vector<std::shared_ptr<XJVulkanImage>> frameColorImages;
            std::shared_ptr<XJVulkanDepthImage> depthImage = nullptr;
            std::shared_ptr<XJVulkanImage> resolveImage = nullptr;
        
            // 1. 创建颜色附件
            for (int colorIdx : colorAttachmentIndices) 
            {
                const Attachment& colorAttach = kAttachments[colorIdx];
                std::shared_ptr<XJVulkanImage> colorImg;

                // 判断是否为多重采样颜色附件
                if (colorAttach.samples > VK_SAMPLE_COUNT_1_BIT) 
                {
                    // 多重采样颜色附件：创建新图像（不能使用交换链图像）
                    colorImg = std::make_shared<XJVulkanImage>(
                        kDevice,
                        VkExtent3D{mExtent.width, mExtent.height, 1},
                        colorAttach.format,
                        colorAttach.usage,
                        colorAttach.samples
                    );
                    spdlog::debug("多重采样颜色附件[{}]: 创建新图像，采样数={}", 
                                  colorIdx, colorAttach.samples);
                }
                else 
                {
                    // 单采样颜色附件：检查是否为交换链目标
                    if (bSwapchainTarget && i < kSwapchainImages.size() && 
                        colorAttach.finalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) 
                    {
                        // 交换链图像作为颜色附件
                        colorImg = std::make_shared<XJVulkanImage>(
                            kDevice,
                            kSwapchainImages[i],
                            VkExtent3D{mExtent.width, mExtent.height, 1},
                            colorAttach.format,
                            colorAttach.usage,
                            colorAttach.samples
                        );
                        spdlog::debug("单采样颜色附件[{}]: 使用交换链图像", colorIdx);
                    }
                    else 
                    {
                        // 非交换链目标：创建新图像
                        colorImg = std::make_shared<XJVulkanImage>(
                            kDevice,
                            VkExtent3D{mExtent.width, mExtent.height, 1},
                            colorAttach.format,
                            colorAttach.usage,
                            colorAttach.samples
                        );
                        spdlog::debug("单采样颜色附件[{}]: 创建新图像", colorIdx);
                    }
                }
                
                if (!colorImg->IsValid()) {
                    spdlog::error("颜色附件创建失败: 索引{}", colorIdx);
                    return;
                }
                frameColorImages.push_back(colorImg);
            }
        
            // 2. 创建深度附件（如果有）
            if (!depthAttachmentIndices.empty()) 
            {
                int depthIdx = depthAttachmentIndices[0];
                const Attachment& depthAttach = kAttachments[depthIdx];
                
                VulkanPhysicalDevices* physicalDevices = kRenderContext->XJGetPhysicalDevices();
                if (!physicalDevices) {
                    spdlog::error("获取物理设备失败");
                    return;
                }
                
                depthImage = std::make_shared<XJVulkanDepthImage>(
                    kDevice,
                    physicalDevices,
                    mExtent.width,
                    mExtent.height,
                    depthAttach.samples  // 使用渲染通道指定的采样数
                );
                if (!depthImage->Create() || !depthImage->IsValid()) {
                    spdlog::error("深度图像创建失败，格式: {}, 采样数: {}", 
                                 vk_format_string(depthAttach.format), depthAttach.samples);
                    return;
                }
                spdlog::debug("深度附件创建成功，采样数: {}", depthAttach.samples);
            }
            
        
            // 3. 创建解析附件（如果有）
            if (!resolveAttachmentIndices.empty()) 
            {
                int resolveIdx = resolveAttachmentIndices[0]; // 假设只有一个解析附件
                const Attachment& resolveAttach = kAttachments[resolveIdx];

                // 解析附件使用交换链图像
                if (i < kSwapchainImages.size()) 
                {
                    resolveImage = std::make_shared<XJVulkanImage>(
                        kDevice,
                        kSwapchainImages[i],
                        VkExtent3D{mExtent.width, mExtent.height, 1},
                        resolveAttach.format,
                        resolveAttach.usage,
                        resolveAttach.samples
                    );
                    if (!resolveImage->IsValid()) 
                    {
                        spdlog::warn("解析图像创建失败，将继续使用颜色附件");
                        resolveImage = nullptr; // 设为nullptr，帧缓冲会跳过解析附件
                    }
                    spdlog::debug("解析附件创建成功，使用交换链图像");
                } else {
                    spdlog::error("交换链图像不足");
                    return;
                }
            }
        
            // 创建帧缓冲
            auto framebuffer = std::make_shared<XJVulkanFrameBuffer>(
                kDevice,
                mRenderPass,
                frameColorImages,  // 按索引顺序的颜色附件
                depthImage,        // 可能为nullptr
                resolveImage,      // 可能为nullptr
                mExtent.width,
                mExtent.height
            );

            if (!framebuffer->IsValid()) {
                spdlog::error("帧缓冲创建失败");
                mFrameBuffers.clear();  // 清理已创建的部分
                return;
            }

            mFrameBuffers.push_back(framebuffer);
        }

         // 如果创建失败，恢复原有帧缓冲区
        if (mFrameBuffers.empty()) 
        {
            if (!oldFrameBuffers.empty()) 
            {
                mFrameBuffers = std::move(oldFrameBuffers);
                spdlog::warn("Frame buffer recreation failed, using old buffers");
            } else {
                spdlog::error("Frame buffer creation failed and no old buffers to restore");
            }
        }


    }
    void XJRenderTarget::BeginRenderTarget(VkCommandBuffer commandBuffer)
    {
        assert(!bBeginRenderTarget && "RenderPass must be set before beginning render target.");//确保在开始渲染目标之前设置了RenderPass

         // 检查帧缓冲区是否为空
        if (mFrameBuffers.empty()) 
        {
            spdlog::error("No framebuffers available in render target. FrameBuffer count: {}, Extent: {}x{}", 
                         mBufferCount, mExtent.width, mExtent.height);
            return;
        }
        //spdlog::debug("开始渲染目标：帧缓冲区数量={}, 当前索引={}, 交换链目标={}",
        //          mFrameBuffers.size(), mCurrentBufferIndex, bSwapchainTarget);

        if(bShouldUpdate)
        {
            ReCreate();
            bShouldUpdate = false;
        }
        if(bSwapchainTarget)
        {
             XJRenderContext *renderContext = XJApplication::XJGetAppContext()->renderContext;//`获取渲染上下文
             XJVulkanSwapchain* swapchain = renderContext->XJGetSwapchain();//获取交换链对象

             mCurrentBufferIndex = swapchain->XJGetCurrentImageIndex();//获取当前交换链图片索引
        }
        else
        {
             mCurrentBufferIndex = (mCurrentBufferIndex + 1) % mBufferCount;//循环使用帧缓冲索引
        }
        if (mCurrentBufferIndex >= mFrameBuffers.size()) 
        {
            spdlog::error("Current buffer index {} out of range (max {})", 
                          mCurrentBufferIndex, mFrameBuffers.size());
            mCurrentBufferIndex = 0; // 重置为安全值
        }

        mRenderPass->BeginRenderPass(commandBuffer, XJGetCurrentFrameBuffer(), mClearValues);//开始渲染通道
        bBeginRenderTarget = true;
        
    }
    void XJRenderTarget::EndRenderTarget(VkCommandBuffer commandBuffer)
    {
        if(bBeginRenderTarget)
        {
            mRenderPass->EndRenderPass(commandBuffer);//结束渲染通道
            // 对于交换链目标，确保图像布局正确
            if (bSwapchainTarget) {
                // 如果需要，可以在这里添加图像布局转换命令
                // 但渲染通道的finalLayout应该已经处理了这个转换
                //spdlog::trace("交换链目标渲染结束，图像布局应由渲染通道管理");
            }
            
            bBeginRenderTarget = false;
        }
        
    }
    void XJRenderTarget::SetExtent(const VkExtent2D extent)
    {
        mExtent = extent;
        bShouldUpdate = true;
        
    }
    void XJRenderTarget::SetBufferCount(uint32_t bufferCount)
    {
        mBufferCount = bufferCount;
        bShouldUpdate = true;
    }
    void XJRenderTarget::SetColorClearValue(VkClearColorValue colorClearValue)
    {
        std::vector<Attachment> renderPassAttachments = mRenderPass->XJGetAttachments();
        for (size_t i = 0; i < renderPassAttachments.size(); ++i)
        {
            if(IsDepthStencilFormat(renderPassAttachments[i].format) && renderPassAttachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                mClearValues[i].color = colorClearValue;
            }
            
        }
        
    }
    void XJRenderTarget::SetDepthClearValue(VkClearDepthStencilValue depthClearValue)
    {
        std::vector<Attachment> renderPassAttachments = mRenderPass->XJGetAttachments();
        for (size_t i = 0; i < renderPassAttachments.size(); ++i)
        {
            if(IsDepthStencilFormat(renderPassAttachments[i].format) && renderPassAttachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                mClearValues[i].depthStencil = depthClearValue;
            }
            
        }
        
    }
    void XJRenderTarget::SetColorClearValue(uint32_t attachmentIndex, VkClearColorValue colorClearValue)
    {
        std::vector<Attachment> renderPassAttachments = mRenderPass->XJGetAttachments();
        if(attachmentIndex <= renderPassAttachments.size() - 1)
        {
            if(IsDepthStencilFormat(renderPassAttachments[attachmentIndex].format) && renderPassAttachments[attachmentIndex].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                mClearValues[attachmentIndex].color = colorClearValue;
            }
            
        }
        
    }

    void XJRenderTarget::SetDepthClearValue(uint32_t attachmentIndex, VkClearDepthStencilValue depthClearValue)
    {
        std::vector<Attachment> renderPassAttachments = mRenderPass->XJGetAttachments();
        if(attachmentIndex <= renderPassAttachments.size() - 1)
        {
            if(IsDepthStencilFormat(renderPassAttachments[attachmentIndex].format) && renderPassAttachments[attachmentIndex].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                mClearValues[attachmentIndex].depthStencil = depthClearValue;
            }
            
        }
        
    }
}