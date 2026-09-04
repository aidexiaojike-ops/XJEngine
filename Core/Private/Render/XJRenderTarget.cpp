#include "Render/XJRenderTarget.h"
#include "XJApplication.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanImage.h"
#include "spdlog/spdlog.h"
#include "Graphic/XJVulkanDepthImage.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"
#include "ECS/Component/XJCameraComponent.h"

#include <stdexcept>

namespace XJ
{
    XJRenderTarget::XJRenderTarget(XJVulkanRenderPass *rederPass)
    {
        if (!rederPass)
        {
            spdlog::error("XJRenderTarget constructor failed: render pass is null.");
            throw std::invalid_argument("XJRenderTarget requires a valid render pass");
        }

        XJRenderContext* renderContext = XJApplication::XJGetAppContext()->renderContext;
        if (!renderContext)
        {
            spdlog::error("XJRenderTarget constructor failed: render context is null.");
            throw std::runtime_error("XJRenderTarget requires a valid render context");
        }

        XJVulkanSwapchain* swapchain = renderContext->XJGetSwapchain();
        if (!swapchain)
        {
            spdlog::error("XJRenderTarget constructor failed: swapchain is null.");
            throw std::runtime_error("XJRenderTarget requires a valid swapchain");
        }

        mRenderPass = rederPass;
        mBufferCount = static_cast<uint32_t>(swapchain->XJGetSwapchainImages().size());
        mExtent = {swapchain->XJGetWidth(), swapchain->XJGetHeight()};
        bSwapchainTarget = true;

        Init();
        ReCreate();

        if (mFrameBuffers.empty())
        {
            spdlog::error("XJRenderTarget constructor failed: no framebuffer was created.");
            throw std::runtime_error("XJRenderTarget failed to create framebuffers");
        }
        
    }
    XJRenderTarget::XJRenderTarget(XJVulkanRenderPass *rederPass,uint32_t bufferCount, VkExtent2D extent)
                :   mRenderPass(rederPass), mBufferCount(bufferCount), mExtent(extent), bSwapchainTarget(false)
    {
        if (!mRenderPass)
        {
            spdlog::error("XJRenderTarget constructor failed: render pass is null.");
            throw std::invalid_argument("XJRenderTarget requires a valid render pass");
        }

        if (mBufferCount == 0 || mExtent.width == 0 || mExtent.height == 0)
        {
            spdlog::error(
                "XJRenderTarget constructor failed: invalid buffer count or extent. count={}, extent={}x{}",
                mBufferCount,
                mExtent.width,
                mExtent.height);
            throw std::invalid_argument("XJRenderTarget requires non-zero buffer count and extent");
        }

        Init();
        ReCreate();

        if (mFrameBuffers.empty())
        {
            spdlog::error("XJRenderTarget constructor failed: no framebuffer was created.");
            throw std::runtime_error("XJRenderTarget failed to create framebuffers");
        }
    }
    XJRenderTarget::~XJRenderTarget()
    {
        for(const auto &item: mMaterialSystemList)
        {
            item->OnDestroy();
        }
        mMaterialSystemList.clear();
    }

    void XJRenderTarget::XJSetCamera(XJEntity* camera)
    {
        // 不保存裸指针，只保存 UUID。camera 之后被销毁也不会留下悬垂指针。
        mCameraId = camera ? camera->XJGetUUID() : XJUUID{0};
    }

    void XJRenderTarget::XJClearCamera()
    {
        mCameraId = XJUUID{0};
    }

    XJEntity* XJRenderTarget::XJGetCamera() const
    {
        if(!mCameraId)
            return nullptr;

        // 优先在自己的 Scene 中解析相机（Play 态下为运行时克隆）；
        // 未显式注入 Scene 的旧 RenderTarget 回退到全局编辑场景。
        XJScene* scene = mScene;

        if (!scene)
        {
            XJAppContext* appContext = XJApplication::XJGetAppContext();
            scene = appContext ? appContext->scene : nullptr;
        }

        if (!scene)
            return nullptr;

        // 从当前 scene 按 UUID 查询。实体已删除或 scene 已卸载时自然返回 nullptr。
        return scene->FindEntityByUUID(mCameraId);
    }

    void XJRenderTarget::Init()
    {
        if (!mRenderPass)
        {
            spdlog::error("XJRenderTarget::Init failed: render pass is null.");
            return;
        }

        const uint32_t attachmentCount = mRenderPass->XJGetAttachmentSize();
        if (attachmentCount == 0)
        {
            spdlog::error("XJRenderTarget::Init failed: render pass has no attachments.");
            return;
        }

        // clear values 数量必须和 render pass attachment 数量一致。
        mClearValues.resize(mRenderPass->XJGetAttachmentSize());
        SetColorClearValue({0.0f, 0.0f, 0.0f, 1.0f});//默认颜色清除值为黑色
        SetDepthClearValue({1.0f, 0});//默认深度清除值为1.0，模板清除值为0
        
    }
    void XJRenderTarget::UpdateIfNeeded()
    {
        if (bShouldUpdate)
        {
            ReCreate();
            bShouldUpdate = false;
        }
    }
    void XJRenderTarget::ReCreate()
    {
        if (mExtent.width == 0 || mExtent.height == 0 || mBufferCount == 0)
        {
            spdlog::error("Invalid render target extent or buffer count. ReCreate aborted.");
            return;
        }
        
        // 检查渲染通道是否有效
        if (!mRenderPass) {
            spdlog::error("Render pass is null in ReCreate");
            return;
        }
 


        XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        if (!kRenderContext || !kRenderContext->XJGetDevice())
        {
            spdlog::error("Render context or device is null in ReCreate");
            return;
        }

        VkResult idleRet = vkDeviceWaitIdle(kRenderContext->XJGetDevice()->XJGetDevice());
        if (idleRet != VK_SUCCESS)
        {
            spdlog::critical("STEP3-FAIL: ReCreate vkDeviceWaitIdle returned {}", vk_result_string(idleRet));
        }


        XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();
        XJVulkanSwapchain* kSwapchain = kRenderContext->XJGetSwapchain();

        auto oldFrameBuffers = std::move(mFrameBuffers);
        auto oldDepthImages = std::move(mDepthImages);
     
        mFrameBuffers.clear();
        mDepthImages.clear();
        mFrameBuffers.reserve(mBufferCount);

        //vkDeviceWaitIdle(kRenderContext->XJGetDevice()->XJGetDevice());//// 等待设备空闲，确保无命令缓冲区使用旧资源
        std::vector<Attachment> kAttachments = mRenderPass->XJGetAttachments();
        if(kAttachments.empty())
        {
            spdlog::error("Render pass has no attachments. ReCreate aborted.");
            mFrameBuffers = std::move(oldFrameBuffers);
            mDepthImages = std::move(oldDepthImages);
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
                             idx, vk_format_string(attach.format), static_cast<int>(attach.samples));
            }
            else 
            {
                colorAttachmentIndices.push_back(idx);
                spdlog::debug("颜色附件[{}]: format={}, samples={}",
                             idx, vk_format_string(attach.format), static_cast<int>(attach.samples));
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
                          vk_image_layout_string(attach.finalLayout), static_cast<int>(attach.samples));
        }
        
        // 验证附件数量与渲染通道匹配
        if (colorAttachmentIndices.empty()) {
            spdlog::error("没有找到颜色附件");
            mFrameBuffers = std::move(oldFrameBuffers);
            mDepthImages = std::move(oldDepthImages);
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
                                  colorIdx, static_cast<int>(colorAttach.samples));
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
                    mFrameBuffers = std::move(oldFrameBuffers);
                    mDepthImages = std::move(oldDepthImages);
                    return;
                }
                frameColorImages.push_back(colorImg);
            }
        
            // 2. 创建深度附件（如果有）
            if (!depthAttachmentIndices.empty()) 
            {
                int depthIdx = depthAttachmentIndices[0];
                const Attachment& depthAttach = kAttachments[depthIdx];
                
                XJVulkanPhysicalDevices* physicalDevices = kRenderContext->XJGetPhysicalDevices();
                if (!physicalDevices) {
                    spdlog::error("获取物理设备失败");
                    mFrameBuffers = std::move(oldFrameBuffers);
                    mDepthImages = std::move(oldDepthImages);
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
                                 vk_format_string(depthAttach.format), static_cast<int>(depthAttach.samples));
                    mFrameBuffers = std::move(oldFrameBuffers);
                    mDepthImages = std::move(oldDepthImages);
                    return;
                }
                spdlog::debug("深度附件创建成功，采样数: {}, 函数:{}", static_cast<int>(depthAttach.samples), __FILE__);
                mDepthImages.push_back(depthImage);
            }
            
        
            // 3. 创建解析附件（如果有）
            if (!resolveAttachmentIndices.empty())
            {
                int resolveIdx = resolveAttachmentIndices[0]; // 当前 framebuffer API 只支持一个 resolve image
                const Attachment& resolveAttach = kAttachments[resolveIdx];
            
                if (bSwapchainTarget)
                {
                    // 交换链目标：MSAA resolve 最终写入当前 swapchain image。
                    if (i >= kSwapchainImages.size())
                    {
                        spdlog::error("Swapchain image count is smaller than render target buffer count.");
                        mFrameBuffers = std::move(oldFrameBuffers);
                        mDepthImages = std::move(oldDepthImages);
                        return;
                    }
                
                    resolveImage = std::make_shared<XJVulkanImage>(
                        kDevice,
                        kSwapchainImages[i],
                        VkExtent3D{mExtent.width, mExtent.height, 1},
                        resolveAttach.format,
                        resolveAttach.usage,
                        resolveAttach.samples);
                }
                else
                {
                    // 离屏目标不能使用 swapchain image。resolve attachment 必须是自己的单采样图像，
                    // 否则预览/离屏 framebuffer 会错误绑定窗口后备图像。
                    resolveImage = std::make_shared<XJVulkanImage>(
                        kDevice,
                        VkExtent3D{mExtent.width, mExtent.height, 1},
                        resolveAttach.format,
                        resolveAttach.usage,
                        resolveAttach.samples);
                }
            
                if (!resolveImage || !resolveImage->IsValid())
                {
                    spdlog::error("Resolve attachment image creation failed: index={}", resolveIdx);
                    mFrameBuffers = std::move(oldFrameBuffers);
                    mDepthImages = std::move(oldDepthImages);
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
                break; // 退出循环，避免继续创建无效帧缓冲
            }

            mFrameBuffers.push_back(framebuffer);
        }
         // 如果创建失败，恢复原有帧缓冲区
        if (mFrameBuffers.empty()) 
        {
            if (!oldFrameBuffers.empty()) 
            {
                mFrameBuffers = std::move(oldFrameBuffers);
                mDepthImages = std::move(oldDepthImages);
                spdlog::warn("帧缓冲重建失败，使用旧缓冲区");
            } else {
                spdlog::error("帧缓冲创建失败且无旧缓冲区可恢复");
            }
        }


    }
    bool XJRenderTarget::BeginRenderTarget(VkCommandBuffer commandBuffer)
    {
        if (!commandBuffer)
        {
            spdlog::error("BeginRenderTarget failed: command buffer is null.");
            return false;
        }

        if (!mRenderPass)
        {
            spdlog::error("BeginRenderTarget failed: render pass is null.");
            return false;
        }

        if (bBeginRenderTarget)
        {
            spdlog::error("BeginRenderTarget failed: render target is already begun.");
            return false;
        }

        if (bShouldUpdate)
        {
            // 不在命令录制路径里 ReCreate。ReCreate 内部会 vkDeviceWaitIdle，
            // 如果 BeginRenderTarget 隐式触发，会造成帧内整管线停顿。
            spdlog::warn("BeginRenderTarget skipped: render target needs update. Call UpdateIfNeeded at frame boundary.");
            return false;
        }

        if (mFrameBuffers.empty())
        {
            spdlog::error("BeginRenderTarget failed: no framebuffers available.");
            return false;
        }

        XJEntity* camera = XJGetCamera();
        if (XJEntity::HasComponent<XJCameraComponent>(camera) && mExtent.width > 0 && mExtent.height > 0)
        {
            camera->GetComponent<XJCameraComponent>().XJSetAspectRatio(
                mExtent.width * 1.0f / mExtent.height);
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
        // 获取当前帧缓冲区并验证有效性
        XJ::XJVulkanFrameBuffer* kFrameBuffer =  XJGetCurrentFrameBuffer();
        if(!kFrameBuffer || !kFrameBuffer -> IsValid())
        {
            spdlog::error("无效的帧缓冲区，跳过渲染通道开始");
            bBeginRenderTarget = false;
            return false; // 不设置 bBeginRenderTarget = true
        }
        // 尝试开始渲染通道
        if(mRenderPass->BeginRenderPass(commandBuffer, kFrameBuffer, mClearValues))
        {
            bBeginRenderTarget = true;
        }
        else
        {
            spdlog::error("渲染通道开始失败");
            bBeginRenderTarget = false;
        }

        return bBeginRenderTarget;
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
    //同步缓冲区计数
    void XJRenderTarget::SetExtent(const VkExtent2D extent)
    {
        if (extent.width == 0 || extent.height == 0)
        {
            spdlog::warn("SetExtent ignored: invalid extent {}x{}", extent.width, extent.height);
            return;
        }

        XJRenderContext* renderContext = XJApplication::XJGetAppContext()->renderContext;

        if (bSwapchainTarget)
        {
            if (!renderContext || !renderContext->XJGetSwapchain())
            {
                spdlog::error("SetExtent failed: render context or swapchain is null.");
                return;
            }

            XJVulkanSwapchain* swapchain = renderContext->XJGetSwapchain();
            mBufferCount = static_cast<uint32_t>(swapchain->XJGetSwapchainImages().size());
        }

        if (mExtent.width == extent.width && mExtent.height == extent.height)
            return;

        mExtent = extent;
        bShouldUpdate = true;
        
    }
    void XJRenderTarget::SetBufferCount(uint32_t bufferCount)
    {
        if (bufferCount == 0)
        {
            spdlog::warn("SetBufferCount ignored: bufferCount is zero.");
            return;
        }

        if (mBufferCount == bufferCount)
            return;

        mBufferCount = bufferCount;
        bShouldUpdate = true;
    }
    void XJRenderTarget::SetColorClearValue(VkClearColorValue colorClearValue)
    {
        if (!mRenderPass)
        {
            spdlog::error("SetColorClearValue failed: render pass is null.");
            return;
        }

        std::vector<Attachment> renderPassAttachments = mRenderPass->XJGetAttachments();
        for (size_t i = 0; i < renderPassAttachments.size(); ++i)
        {
            if(!IsDepthStencilFormat(renderPassAttachments[i].format) && renderPassAttachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                mClearValues[i].color = colorClearValue;
            }
            
        }
        
    }
    void XJRenderTarget::SetDepthClearValue(VkClearDepthStencilValue depthClearValue)
    {
        if (!mRenderPass)
        {
            spdlog::error("SetColorClearValue failed: render pass is null.");
            return;
        }

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
        if (!mRenderPass)
        {
            spdlog::error("SetColorClearValue failed: render pass is null.");
            return;
        }

        std::vector<Attachment> renderPassAttachments = mRenderPass->XJGetAttachments();
        if (attachmentIndex >= renderPassAttachments.size() || attachmentIndex >= mClearValues.size())
            return;

        if (!IsDepthStencilFormat(renderPassAttachments[attachmentIndex].format) &&
            renderPassAttachments[attachmentIndex].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        {
            mClearValues[attachmentIndex].color = colorClearValue;
        }
        
    }

    void XJRenderTarget::SetDepthClearValue(uint32_t attachmentIndex, VkClearDepthStencilValue depthClearValue)
    {
        if (!mRenderPass)
        {
            spdlog::error("SetDepthClearValue failed: render pass is null.");
            return;
        }
    
        std::vector<Attachment> renderPassAttachments = mRenderPass->XJGetAttachments();
        if (attachmentIndex >= renderPassAttachments.size() || attachmentIndex >= mClearValues.size())
            return;
    
        if (IsDepthStencilFormat(renderPassAttachments[attachmentIndex].format) &&
            renderPassAttachments[attachmentIndex].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        {
            mClearValues[attachmentIndex].depthStencil = depthClearValue;
        }
        
    }

    void XJRenderTarget::SetScene(XJScene* scene)
    {
        mScene = scene;

        for(auto& system : mMaterialSystemList)
        {
            if (system)
                system->SetScene(scene);
        }
    }
}
