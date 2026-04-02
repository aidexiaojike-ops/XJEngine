#include "XJEntryPoint.h"
#include "Render/XJRenderTarget.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanCommandBuffer.h"
#include "Graphic/XJVulkanGeometryUtil.h"
#include "Edit/FileUtil.h"
#include "Render/XJMesh.h"
#include "Render/XJRenderer.h"
#include "Graphic/VulkanCommon.h"
#include "Graphic/XJVulkanDescriptorSet.h"
#include "ECS/System/XJBaseMaterialSystem.h"

#include "ECS/XJScene.h"

#include <chrono>

struct PushConstants
{
     glm::mat4 matrix{1.0f}; // 4x4 矩阵，默认初始化为单位矩阵
};// 推送常量结构体
   
    
class XJEngineApp : public XJ::XJApplication
{
protected:

    void  OnConfiguration(XJ::AppSettings *appSettings) override
    {
        // 可以在这里修改默认的应用程序设置，例如窗口标题、初始窗口大小等
        appSettings->windowWidth = 1600;
        appSettings->windowHeight = 1200;
        appSettings->title = "XJEngine Application";
    }
    void OnInit() override
    {

        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();
        XJ::VulkanPhysicalDevices* kPhysicalDevices = kRenderContext->XJGetPhysicalDevices();
        XJ::XJVulkanSwapchain* kSwapchain = kRenderContext->XJGetSwapchain();
      

         //renderpass渲染通道配置
        std::vector<XJ::Attachment> attachments{};
        attachments.resize(2);
        //颜色附件
        attachments[0].flags = 0;
        attachments[0].format = kSwapchain->XJGetSurfaceInfo().surfaceFormat.format;//表面格式    颜色附加用surfaceFormat同样的格式
        attachments[0].samples = mSampleCount;//采样数
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//加载操作 清除
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;//存储操作 存储
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;//模板加载操作 不关心
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;//模板存储操作 不关心
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//初始布局 未定义
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//显示布局
        //深度附件
        attachments[1].flags = 0;
        attachments[1].format = kDevice->XJGetSettings().depthFormat;//深度格式
        attachments[1].samples = mSampleCount;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;//深度附加布局


      
        //子通道
        std::vector<XJ::RenderSubPass> subpasses = //多重采样
        {
            {
                .colorAttachments = { 0 }, //颜色附件索引
                .depthStencilAttachments = { 1 },//深度附件索引
                .resolveAttachments = {},   // 解析附件由渲染通道自动添加
                .sampleCount = mSampleCount//采样数
                // .sampleCount = VK_SAMPLE_COUNT_1_BIT//采样数
            }
        };


        mRenderPass = std::make_shared<XJ::XJVulkanRenderPass>(kDevice, kPhysicalDevices, attachments, subpasses);//创建渲染通道对象
        spdlog::info("渲染通道创建成功");
        mRenderTarget = std::make_shared<XJ::XJRenderTarget>(mRenderPass.get());

        mRenderTarget->SetColorClearValue(0, VkClearColorValue{0.1f, 0.1f, 0.1f, 1.0f});//设置颜色清除值
        mRenderTarget->SetDepthClearValue(VkClearDepthStencilValue{1.0f, 0});//设置深度清除值
        mRenderTarget->AddMaterialSystem<XJ::XJBaseMaterialSystem>();

        mRender = std::make_shared<XJ::XJRenderer>();

        //descriptor set   绑定shader
/*
        std::vector<VkDescriptorSetLayoutBinding> kDesctLayoutBindings
        {
            
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
            {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
            {
                .binding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            }
        };

        mDescriptorSetLayout = std::make_shared<XJ::XJVulkanDescriptorSetLayout>(kDevice, kDesctLayoutBindings);

    

        std::vector<VkDescriptorPoolSize> poolSizes = 
        {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .descriptorCount = 2
            },
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 2
            },
        };

        mDescriptorPool = std::make_shared<XJ::XJVulkanDescriptorPool>(kDevice, 1, poolSizes);
        mDescriptorSets = mDescriptorPool->AllocateDescriptorSet(mDescriptorSetLayout.get(), 1);
        //buffer的资源准备
        mGlobalBuffer = std::make_shared<XJ::XJVulkanBuffer>(kDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(mGlobalUbo),nullptr,true);
        mInstanceBuffer = std::make_shared<XJ::XJVulkanBuffer>(kDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(mInstanceUbo),nullptr,true);
        //贴图
        mTextureA = std::make_shared<XJ::XJTexture>(XJ_RES_TEXTURE_DIR"R.png");
        mTextureB = std::make_shared<XJ::XJTexture>(XJ_RES_TEXTURE_DIR"R-C.jpeg");

*/
         // 创建命令池
        mCommandBuffers = kDevice->XJGetDefaultCmdPool()->AllocateCommandBuffer(static_cast<uint32_t>(kSwapchain->XJGetSwapchainImages().size()));//分配命令缓冲区
        spdlog::info("分配了 {} 个命令缓冲区", mCommandBuffers.size());

         //geometry util 
        std::vector<XJ::XJVulkanVertex> mVertices;
        std::vector<uint32_t> mIndices;
        mGeometryUtil = std::make_unique<XJ::XJVulkanGeometryUtil>();
        mGeometryUtil->CreateCube(-0.5f, 0.5f, -0.5f, 0.5f, -0.5f, 0.5f, mVertices, mIndices, true, true, glm::mat4(1.0f));
        spdlog::info("创建立方体: {} 个顶点, {} 个索引", mVertices.size(), mIndices.size());
        //创建网格对象
        mMesh = std::make_shared<XJ::XJMesh>(mVertices, mIndices);

    }

    void OnSceneInit(XJ::XJScene *scene) override
    {
        //在这里可以添加场景初始化的代码，例如创建实体、添加组件等
        spdlog::info("场景初始化");
        {
            XJ::XJEntity* kCube = scene->CreateEntity("CubeEntityA");//创建一个实体
            auto &kTransformComp = kCube->GetComponent<XJ::XJTransformComponent>();//添加变换组件
            auto &kMaterialComp = kCube->AddComponent<XJ::XJBaseMaterialComponent>();//添加材质组件
            auto &kMeshComp = kCube->AddComponent<XJ::XJMeshComponent>();//添加网格组件

            kMeshComp.mMesh = mMesh.get();//设置网格组件的网格对象

            kTransformComp.position = glm::vec3(0.0f, 1.0f, 0.0f);
            kTransformComp.rotation = glm::vec3(30.0f, 45.0f, 0.0f);
            kTransformComp.scale = glm::vec3(0.25f, 0.25f, 0.25f);//设置变换组件的缩放
            kTransformComp.UpdateModelMatrix(); // 新增：立即更新模型矩阵
        }
        {
            XJ::XJEntity* kCube = scene->CreateEntity("CubeEntityB");//创建一个实体
            auto &kTransformComp = kCube->GetComponent<XJ::XJTransformComponent>();//添加变换组件
            auto &kMaterialComp = kCube->AddComponent<XJ::XJBaseMaterialComponent>();//添加材质组件
            auto &kMeshComp = kCube->AddComponent<XJ::XJMeshComponent>();//添加网格组件
            
            kMeshComp.mMesh = mMesh.get();//设置网格组件的网格对象
            
            kTransformComp.position = glm::vec3(0.0f, 0.0f, 0.0f);
            kTransformComp.rotation = glm::vec3(30.0f, 45.0f, 0.0f);
            kTransformComp.scale = glm::vec3(0.25f, 0.25f, 0.25f);//设置变换组件的缩放
            kTransformComp.UpdateModelMatrix(); // 新增：立即更新模型矩阵
        }
        {
            XJ::XJEntity* kCube = scene->CreateEntity("CubeEntityC");//创建一个实体
            auto &kTransformComp = kCube->GetComponent<XJ::XJTransformComponent>();//添加变换组件
            auto &kMaterialComp = kCube->AddComponent<XJ::XJBaseMaterialComponent>();//添加材质组件
            auto &kMeshComp = kCube->AddComponent<XJ::XJMeshComponent>();//添加网格组件
            
            kMeshComp.mMesh = mMesh.get();//设置网格组件的网格对象
            
            kTransformComp.position = glm::vec3(1.0f, 0.5f, 0.0f);
            kTransformComp.rotation = glm::vec3(30.0f, 45.0f, 0.0f);
            kTransformComp.scale = glm::vec3(0.25f, 0.25f, 0.25f);//设置变换组件的缩放
            kTransformComp.UpdateModelMatrix(); // 新增：立即更新模型矩阵
        }
    }
    void OnSceneDestroy(XJ::XJScene *scene) override
    {
        //在这里可以添加场景销毁的代码，例如清理资源等
        spdlog::info("场景销毁");
    }

    void OnUpdate(float deltaTime) override
    {
        /*
        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanSwapchain* kSwapchain = kRenderContext->XJGetSwapchain();
          //更新推送常量  旋转矩阵
        float kTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - mStartTimePoint).count();
        mInstanceUbo.modelMat = glm::rotate(glm::mat4(1.0f), glm::radians(17.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        mInstanceUbo.modelMat = glm::rotate(mInstanceUbo.modelMat, glm::radians(45.0f * kTime), glm::vec3(0.0f, 1.0f, 0.0f));

        //透视投影矩阵
        //mGlobalUbo.projMat = glm::perspectiveRH_ZO(glm::radians(65.0f), static_cast<float>(kSwapchain->XJGetWidth()) * 1.0f / static_cast<float>(kSwapchain->XJGetHeight()), 0.1f, 100.0f);
        mGlobalUbo.projMat = glm::perspective(glm::radians(65.0f), static_cast<float>(kSwapchain->XJGetWidth()) * 1.0f / static_cast<float>(kSwapchain->XJGetHeight()), 0.1f, 100.0f);
        mGlobalUbo.projMat[1][1] *= -1;  // 取消注释，启用 Y 轴翻转
        mGlobalUbo.viewMat = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        */

    }
    
    void OnRender() override
    {
        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();
        XJ::XJVulkanSwapchain* kSwapchain = kRenderContext->XJGetSwapchain();
        
        int32_t imageIndex;//拿image
        if(mRender->XJRendererBegin(&imageIndex, mCommandBuffers))
        {
            mRenderTarget->SetExtent({kSwapchain->XJGetWidth(),kSwapchain->XJGetHeight()});
        }


        VkCommandBuffer kCommandBuffer = mCommandBuffers[imageIndex];//拿到commandbuffer
        if (kCommandBuffer == VK_NULL_HANDLE) 
        {
            spdlog::error("命令缓冲区句柄为空");
            return;
        }
        //启动命令缓冲 commandbuffer 
        XJ::XJVulkanCommandPool::BeginCommandBuffer(kCommandBuffer);

        //正视图 RT
        mRenderTarget->BeginRenderTarget(kCommandBuffer);//开始渲染通道
/*
        mPipeline->BindPipeline(kCommandBuffer);//绑定管线
        XJ::XJVulkanFrameBuffer *kFrameBuffer = mRenderTarget->XJGetCurrentFrameBuffer();
        //设置视口和裁剪矩形
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(kFrameBuffer->XJGetWidth());
        viewport.height = static_cast<float>(kFrameBuffer->XJGetHeight());
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(kCommandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {kFrameBuffer->XJGetWidth(), kFrameBuffer->XJGetHeight()};
        vkCmdSetScissor(kCommandBuffer, 0, 1, &scissor);

        mGlobalBuffer->WriteData(&mGlobalUbo);
        mInstanceBuffer->WriteData(&mInstanceUbo);
        UpdataDescriptorSets(kCommandBuffer);

        vkCmdBindDescriptorSets(kCommandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS, 
            mPipelineLayout->XJGetPipelineLayout(), 0, 1, mDescriptorSets.data(), 0, nullptr);
          //推送常量
        //vkCmdPushConstants(kCommandBuffer, mPipelineLayout->XJGetPipelineLayout(),
        //    VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mPushConstants), &mPushConstants);
            
        mMesh->Draw(kCommandBuffer);//绘制网格
*/
        mRenderTarget->RenderMaterialSystem(kCommandBuffer);//便利系统
        mRenderTarget->EndRenderTarget(kCommandBuffer);//结束渲染通道

        // 侧视图  Side view
        //mRenderTargetSide->BeginRenderTarget(kCommandBuffer);//开始渲染通道
        //mRenderTargetSide->RenderMaterialSystem(kCommandBuffer);//便利系统
        //mRenderTargetSide->EndRenderTarget(kCommandBuffer);//结束渲染通道

        XJ::XJVulkanCommandPool::EndCommandBuffer(kCommandBuffer);
        //提交命令缓冲区 - 使用 mSubmitFences 作为提交围栏
        if(mRender->XJRendererEnd(imageIndex, {kCommandBuffer}))
        {
            mRenderTarget->SetExtent({kSwapchain->XJGetWidth(),kSwapchain->XJGetHeight()});
           
        }
    }
    void OnDestroy() override
    {
        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();
        vkDeviceWaitIdle(kDevice->XJGetDevice());//等待设备空闲
        /*
        mGlobalBuffer.reset();
        mInstanceBuffer.reset();
        mTextureA.reset();
        mTextureB.reset();
        */
        mMesh.reset();
        mCommandBuffers.clear();
        /*
        mDescriptorSetLayout.reset();
        mDescriptorPool.reset();
        mPipeline.reset();
        mPipelineLayout.reset();
        */
        mRenderTarget.reset();
        mRenderPass.reset();

        mRender.reset();
    }
/*
    void UpdataDescriptorSets(VkCommandBuffer cmdBuffer)
    {
        XJ::XJRenderContext *kRenderContext = XJ::XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();

        VkDescriptorBufferInfo globalBufferInfo{};
        globalBufferInfo.buffer = mGlobalBuffer->XJGetBuffer();
        globalBufferInfo.offset = 0;
        globalBufferInfo.range = sizeof(mGlobalUbo);

        VkDescriptorBufferInfo instanceBufferInfo{};
        instanceBufferInfo.buffer = mInstanceBuffer->XJGetBuffer();
        instanceBufferInfo.offset = 0;
        instanceBufferInfo.range = sizeof(mInstanceUbo);

        VkDescriptorImageInfo textureAImageBufferInfo{};
        textureAImageBufferInfo.sampler = mTextureA->XJGetSampler();
        textureAImageBufferInfo.imageView = mTextureA->XJGetImageView()->XJGetImageView();
        textureAImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo textureBImageBufferInfo{};
        textureBImageBufferInfo.sampler = mTextureB->XJGetSampler();
        textureBImageBufferInfo.imageView = mTextureB->XJGetImageView()->XJGetImageView();
        textureBImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorSet descriptorSet = mDescriptorSets[0];

        std::vector<VkWriteDescriptorSet> writeDescriptorSet = 
        {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = descriptorSet,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &globalBufferInfo
                
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = descriptorSet,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &instanceBufferInfo
                
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = descriptorSet,
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo  = &textureAImageBufferInfo
                
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = descriptorSet,
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo  = &textureBImageBufferInfo
                
            }
        };

        

        vkUpdateDescriptorSets(kDevice->XJGetDevice(),
        writeDescriptorSet.size(), writeDescriptorSet.data(), 0, nullptr);

    }
*/
   
private:
    std::shared_ptr<XJ::XJVulkanRenderPass>             mRenderPass;
    std::shared_ptr<XJ::XJRenderTarget>                 mRenderTarget;
    std::shared_ptr<XJ::XJRenderer>                     mRender;

    //std::shared_ptr<XJ::XJVulkanDescriptorSetLayout>    mDescriptorSetLayout;
    //std::shared_ptr<XJ::XJVulkanDescriptorPool>         mDescriptorPool;
    //std::vector<VkDescriptorSet>                        mDescriptorSets;

    //std::shared_ptr<XJ::XJVulkanPipelineLayout>         mPipelineLayout;
    //std::shared_ptr<XJ::XJVulkanPipeline>               mPipeline;
    std::vector<VkCommandBuffer>                        mCommandBuffers;
    std::shared_ptr<XJ::XJVulkanGeometryUtil>           mGeometryUtil;
    std::shared_ptr<XJ::XJMesh>                         mMesh;

    VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT; // 多重采样数量

    // PushConstants mPushConstants{};//推送常量实例
    
    //GlobalUbo mGlobalUbo;
    //InstanceUbo mInstanceUbo;
    //std::shared_ptr<XJ::XJVulkanBuffer> mGlobalBuffer;
    //std::shared_ptr<XJ::XJVulkanBuffer> mInstanceBuffer;
    //std::shared_ptr<XJ::XJTexture> mTextureA;
    //std::shared_ptr<XJ::XJTexture> mTextureB;


};

XJ::XJApplication* CreateApplicationEntryPoint()
{
    return new XJEngineApp();
}
