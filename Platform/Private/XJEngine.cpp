/*
#include "XJEngine.h"

namespace XJ
{
    XJEngine::XJEngine() 
    {
        mSpdlogDebug = std::make_unique<SpdlogDebug>();//创建日志对象
        mWindow = std::make_unique<XJGlfwWindow>();//创建窗口对象
        mInstance = std::make_unique<VulkanInstance>();//创建Vulkan实例对象
        mSurface = std::make_unique<VulkanSurface>(mWindow.get(), mInstance.get());//创建表面对象
        mPhysicalDevices = std::make_unique<VulkanPhysicalDevices>(mInstance.get(), mSurface.get());//创建物理设备对象
        mDevice = std::make_unique<XJVulkanDevice>(mPhysicalDevices.get(), 1, 1);//创建逻辑设备对象  图形队列索引  显示队列索引
        mSwapchain = std::make_unique<XJVulkanSwapchain>(mPhysicalDevices.get(), mDevice.get(), mSurface.get());//创建交换链对象

        // 创建交换链
        if (!mSwapchain->ReCreate()) {
            throw std::runtime_error("交换链创建失败");
        }
        spdlog::info("交换链创建成功: {}x{}, 图像数量: {}", mSwapchain->XJGetWidth(), mSwapchain->XJGetHeight(), mSwapchain->XJGetSwapchainImages().size());
        
        //renderpass渲染通道配置
        std::vector<Attachment> attachments{};
        attachments.resize(2);
        //颜色附件
        attachments[0].flags = 0;
        attachments[0].format = mDevice->XJGetSettings().surfaceFormat;//表面格式    颜色附加用surfaceFormat同样的格式
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;//采样数
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//加载操作 清除
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;//存储操作 存储
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;//模板加载操作 不关心
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;//模板存储操作 不关心
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//初始布局 未定义
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//显示布局

        // 获取第一个深度图像的格式（假设所有深度图像格式相同）
        if (!mDepthImages.empty()) {
            kDepthFormat = mDepthImages[0]->XJGetFormat();
        }
        //深度附件
        attachments[1].flags = 0;
        attachments[1].format = kDepthFormat;//深度格式
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;//深度附加布局

        //子通道
        std::vector<XJ::RenderSubPass> subpasses{};
        subpasses.resize(1);
        subpasses[0].colorAttachments = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}; //颜色附件索引
        subpasses[0].depthStencilAttachments = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}; //深度附件索

        mRenderPass = std::make_unique<XJVulkanRenderPass>(mDevice.get(), mPhysicalDevices.get(), attachments, subpasses);//创建渲染通道对象
        spdlog::info("渲染通道创建成功");

        mSwapchainImages =  mSwapchain->XJGetSwapchainImages();//获取交换链图片数组
        kSwapchainImageSize = static_cast<uint32_t>(mSwapchainImages.size());//交换链图片数量
        kImageExtent = { mSwapchain->XJGetWidth(), mSwapchain->XJGetHeight(), 1 };//交换链图片的宽度和高度
        spdlog::info("交换链图像数量: {}", kSwapchainImageSize);

        // 创建深度图像（每个帧缓冲一个）
        for (uint32_t i = 0; i < kSwapchainImageSize; i++) {
            try {
                // 创建深度图像
                auto depthImage = std::make_shared<XJVulkanDepthImage>(
                    mDevice.get(),
                    mPhysicalDevices.get(),
                    kImageExtent.width,                 // ✅ uint32_t width
                    kImageExtent.height  
                
                );
                
                // 关键：调用 Create() 执行实际 Vulkan 资源分配
                if (!depthImage->Create()) {
                    throw std::runtime_error("深度图像 Create 失败");
                }
                if (!depthImage->IsValid()) 
                {
                    spdlog::error("深度图像创建失败，图像无效");
                    throw std::runtime_error("深度图像创建失败");
                }
                
                mDepthImages.push_back(depthImage);
                spdlog::debug("深度图像 {} 创建成功", i);
                
            } catch (const std::exception& e) {
                spdlog::error("创建深度图像 {} 失败: {}", i, e.what());
                throw;
            }
        }

        for(int i = 0; i< kSwapchainImageSize; i++) //创建深度图片
        {
            try {
                // 颜色图像向量（只包含颜色附件）
                std::vector<std::shared_ptr<XJVulkanImage>> colorImages;
                std::vector<std::shared_ptr<XJVulkanImage>> frameImages{};
                frameImages.resize(2);//每个帧缓冲有两个附件

                // 颜色附件图像
                auto colorImage = std::make_shared<XJVulkanImage>(
                    mDevice.get(),
                    mSwapchainImages[i],               // 交换链图像
                    kImageExtent,
                    mDevice->XJGetSettings().surfaceFormat,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                );
                colorImages.push_back(colorImage);

                 // 深度附件图像
                if (i >= mDepthImages.size()) {
                    throw std::runtime_error("深度图像数量不足");
                }
                  // 创建帧缓冲
                auto framebuffer = std::make_shared<XJVulkanFrameBuffer>(
                    mDevice.get(),
                    mRenderPass.get(),
                    colorImages,            // 颜色图像向量
                    mDepthImages[i],        // 深度图像指针
                    mSwapchain->XJGetWidth(),
                    mSwapchain->XJGetHeight()
                );
                if (!framebuffer->IsValid()) 
                {
                    throw std::runtime_error("帧缓冲创建失败");
                }
                    
                mFrameBuffers.push_back(framebuffer);
                spdlog::debug("帧缓冲 {} 创建成功", i);
            }
         
            catch (const std::exception& e) {
                    spdlog::error("创建帧缓冲 {} 失败: {}", i, e.what());
                    throw;
                }
        }

        
        mShaderLayout.pushConstantRanges.resize(1);//推送常量范围
        mShaderLayout.pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;//顶点着色器阶段
        mShaderLayout.pushConstantRanges[0].offset = 0;//偏移0
        mShaderLayout.pushConstantRanges[0].size = sizeof(PushConstants);//大小为推送常量结构体大小

        mPipelineLayout = std::make_unique<XJVulkanPipelineLayout>(mDevice.get(), //创建管线布局
            XJ_RES_SHADER_DIR"BaseVertex.vert", XJ_RES_SHADER_DIR"BaseVertex.frag", mShaderLayout);//顶点着色器路径  片元着色器路径
        mPipeline = std::make_unique<XJVulkanPipeline>(mDevice.get(), mRenderPass.get(), mPipelineLayout.get());//创建管线对象
        

        /* 下面是要迁移到其他地方的
        // 设置顶点输入状态 - 由于使用gl_VertexIndex，不需要顶点属性
        std::vector<VkVertexInputBindingDescription> vertexBindings{};
        vertexBindings.resize(1);
        vertexBindings[0].binding = 0;//绑定点0
        vertexBindings[0].stride = sizeof(XJVulkanVertex);//步幅为顶点结构体大小
        vertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//每个顶点一个数据

        std::vector<VkVertexInputAttributeDescription> vertexAttributes{};
        vertexAttributes.resize(3);
        //位置属性
        vertexAttributes[0].location = 0;//位置位置0
        vertexAttributes[0].binding = 0;//绑定点0
        vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;//格式  三个32位浮点数
        vertexAttributes[0].offset = offsetof(XJVulkanVertex, position);//位置偏移    
        //纹理坐标属性
        vertexAttributes[1].location = 1;//纹理坐标位置1
        vertexAttributes[1].binding = 0;//绑定点0
        vertexAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;//格式  两个32位浮点数
        vertexAttributes[1].offset = offsetof(XJVulkanVertex, texcoord0);//纹理坐标偏移    
        //法线属性
        vertexAttributes[2].location = 2;//法线位置2
        vertexAttributes[2].binding = 0;//绑定点0
        vertexAttributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;//格式  三个32位浮点数
        vertexAttributes[2].offset = offsetof(XJVulkanVertex, normal);//法线偏移  
        

        // mPipeline->EnableDepthTest();//关闭深度测试
        mPipeline->SetVertexInputState(vertexBindings, vertexAttributes);//设置顶点输入状态
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);//设置输入装配状态 三角形列表
        mPipeline->SetDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
        mPipeline->Create();
        spdlog::info("管线创建成功");

        
        //geometry util
        mGeometryUtil = std::make_unique<XJVulkanGeometryUtil>();
        mGeometryUtil->CreateCube(-0.5f, 0.5f, -0.5f, 0.5f, -0.5f, 0.5f, mVertices, mIndices, true, true, glm::mat4(1.0f));
        spdlog::info("创建立方体: {} 个顶点, {} 个索引", mVertices.size(), mIndices.size());

        mVertexBuffer = std::make_shared<XJVulkanBuffer>(mDevice.get(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sizeof(mVertices[0]) * mVertices.size(), mVertices.data());//创建顶点缓冲区
        mIndexBuffer = std::make_shared<XJVulkanBuffer>(mDevice.get(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sizeof(mIndices[0]) * mIndices.size(), mIndices.data());//创建索引缓冲区

        // 创建命令池
        mCommandPool = std::make_unique<XJVulkanCommandPool>(mDevice.get(), 
            mPhysicalDevices->XJGetGraphicQueueFamilyInfo().queueFamilyIndex);
        mCommandBuffers = mCommandPool->AllocateCommandBuffer(static_cast<uint32_t>(mFrameBuffers.size()));//分配命令缓冲区
        spdlog::info("分配了 {} 个命令缓冲区", mCommandBuffers.size());

        mGraphicQueue = mDevice->XJGetFirstGraphicQueue(); //获取图形队列
        if (!mGraphicQueue) {
                throw std::runtime_error("获取图形队列失败");
            }

        // 创建同步对象
        mImageAvailableSemaphores.resize(kNumBuffer);
        mSubimedSemaphores.resize(kNumBuffer);
        mFrameFences.resize(kNumBuffer);
        // 创建信号量
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreInfo.pNext = nullptr;
        semaphoreInfo.flags = 0;
        // 创建围栏，初始状态为有信号量
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.pNext = nullptr;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < kNumBuffer; i++)
        {
            XJDebug_Log(vkCreateSemaphore(mDevice->XJGetDevice(), &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]));//创建信号量
            XJDebug_Log(vkCreateSemaphore(mDevice->XJGetDevice(), &semaphoreInfo, nullptr, &mSubimedSemaphores[i]));//创建信号量
            XJDebug_Log(vkCreateFence(mDevice->XJGetDevice(), &fenceInfo, nullptr, &mFrameFences[i]));//创建围栏
        }
    
        spdlog::info("引擎初始化完成！");

    }
    
    void XJEngine::EngineRun()
    {
              
        mWindow->PollEvents();
        //等待上一帧完成
        XJDebug_Log(vkWaitForFences(mDevice->XJGetDevice(), 1, &mFrameFences[kCurrentBuffer], VK_TRUE, UINT64_MAX));
        XJDebug_Log(vkResetFences(mDevice->XJGetDevice(), 1, &mFrameFences[kCurrentBuffer]));

        //交换链 获取图片
        int32_t imageIndex = mSwapchain->AcquireImage(mImageAvailableSemaphores[kCurrentBuffer], mFrameFences[kCurrentBuffer]);
        //int32_t imageIndex = mSwapchain->AcquireImage(mImageAvailableSemaphores[kCurrentBuffer]);
        //更新推送常量  旋转矩阵
        float time = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - kLastTimePoint).count();
        kPC.matrix = glm::rotate(glm::mat4(1.0f), glm::radians(-17.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        kPC.matrix = glm::rotate(kPC.matrix, glm::radians(45.0f * time), glm::vec3(0.0f, 1.0f, 0.0f));

        kPC.matrix = glm::ortho(-1.0f,1.0f,-1.0f,1.0f,-1.0f,1.0f)*kPC.matrix;

        //启动命令缓冲 commandbuffer
        XJ::XJVulkanCommandPool::BeginCommandBuffer(mCommandBuffers[imageIndex]);
        //设置视口和裁剪矩形
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(mSwapchain->XJGetWidth());
        viewport.height = static_cast<float>(mSwapchain->XJGetHeight());
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(mCommandBuffers[imageIndex], 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {mSwapchain->XJGetWidth(), mSwapchain->XJGetHeight()};
        vkCmdSetScissor(mCommandBuffers[imageIndex], 0, 1, &scissor);

        //开启渲染流程  绑定framebuffer  帧缓冲
        mRenderPass->BeginRenderPass(mCommandBuffers[imageIndex], mFrameBuffers[imageIndex].get(), clearValues);
        //绑定资源  peplien geometry descriptorset.....
        mPipeline->BindPipeline(mCommandBuffers[imageIndex]);
        //推送常量
        vkCmdPushConstants(mCommandBuffers[imageIndex], mPipelineLayout->XJGetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(kPC), &kPC);
        //绑定顶点缓冲区
        VkBuffer vertexBuffers[] = {mVertexBuffer->XJGetBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(mCommandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);//绑定顶点缓冲区

        //绑定索引缓冲区
        VkBuffer indexBuffer = {mIndexBuffer->XJGetBuffer()};
        vkCmdBindIndexBuffer(mCommandBuffers[imageIndex], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        //绘制 draw
        // vkCmdDraw(mCommandBuffers[imageIndex], 3, 1, 0, 0); // 绘制一个三角形
        vkCmdDrawIndexed(mCommandBuffers[imageIndex], static_cast<uint32_t>(mIndices.size()), 1, 0, 0, 0);//绘制索引图元
        //结束 渲染 end renderpass
        mRenderPass->EndRenderPass(mCommandBuffers[imageIndex]);
        //结束 命令缓冲 end commandbuffer
        XJ::XJVulkanCommandPool::EndCommandBuffer(mCommandBuffers[imageIndex]);
        //提交 命令缓冲 submit commandbuffer 
        //到队列
        mGraphicQueue->Submit({mCommandBuffers[imageIndex]}, {mImageAvailableSemaphores[kCurrentBuffer]}, {mSubimedSemaphores[kCurrentBuffer]}, mFrameFences[kCurrentBuffer]);
        //mGraphicQueue->WaitIdle();
        //显示 present
        mSwapchain->Present(imageIndex, {mSubimedSemaphores[kCurrentBuffer]});
        
        mWindow->SwapBuffer();//交换缓冲区
        //切换到下一个缓冲区
        kCurrentBuffer = (kCurrentBuffer + 1) % kNumBuffer;

    }
    XJEngine::~XJEngine()
    {
        // 等待设备空闲
        if (mDevice) vkDeviceWaitIdle(mDevice->XJGetDevice());

        // 显式销毁交换链（在设备之前）
        mSwapchain.reset();
        
       // 🔧 按正确顺序清理
        for(int i  = 0; i < kNumBuffer; i++)
        {
            vkDeviceWaitIdle(mDevice->XJGetDevice());
            vkDestroySemaphore(mDevice->XJGetDevice(), mImageAvailableSemaphores[i], nullptr);//销毁信号量
            vkDestroySemaphore(mDevice->XJGetDevice(), mSubimedSemaphores[i], nullptr);//销毁信号量
            vkDestroyFence(mDevice->XJGetDevice(), mFrameFences[i], nullptr);//销毁围栏
        }

        spdlog::info("引擎清理完成，程序退出！");
    }
  

    bool XJEngine::EndEngine()
    {
        return mWindow->ShouldClose();
    }
}*/