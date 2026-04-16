#include "XJEntryPoint.h"
#include "Edit/FileUtil.h"
#include "Edit/XJEventTesting.h"
#include "Event/XJWindowEvent.h"

#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanCommandBuffer.h"
#include "Graphic/XJVulkanGeometryUtil.h"
#include "Graphic/VulkanCommon.h"
#include "Graphic/XJVulkanDescriptorSet.h"

#include "Render/XJRenderTarget.h"
#include "Render/XJMesh.h"
#include "Render/XJRenderer.h"
#include "Render/XJMaterial.h"


#include "ECS/System/XJBaseMaterialSystem.h"
#include "ECS/XJScene.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/System/XJCameraControllerSystem.h"

#include <chrono>


    
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
        //attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//显示布局
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//显示布局
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
         // 创建命令池
        mCommandBuffers = kDevice->XJGetDefaultCmdPool()->AllocateCommandBuffer(static_cast<uint32_t>(kSwapchain->XJGetSwapchainImages().size()));//分配命令缓冲区
        spdlog::info("分配了 {} 个命令缓冲区", mCommandBuffers.size());
        //摄像机初始化                                                    鼠标灵敏  平移    缩放   旋转
        mCameraController = std::make_unique<XJ::XJCameraControllerSystem>(0.25f, 0.05f, 0.3f, 0.25f);
        // 滚轮事件回调
        mEventTesting = std::make_shared<XJ::XJEventTesting>();
        mOvserver = std::make_shared<XJ::XJEventObserver>();
        mOvserver ->OnEvent<XJ::XJMouseScrollEvent>([this](const XJ::XJMouseScrollEvent &event)
        {
            XJ::XJEntity *kCameraEntity = mRenderTarget->XJGetCamera();
            if(XJ::XJEntity::HasComponent<XJ::XJCameraComponent>(kCameraEntity))
            {
                mCameraController->OnMouseScroll(event.mYOffset, kCameraEntity);
            }
        });
    
        //geometry util 
        std::vector<XJ::XJVulkanVertex> mVertices;
        std::vector<uint32_t> mIndices;
        mGeometryUtil = std::make_unique<XJ::XJVulkanGeometryUtil>();
        mGeometryUtil->CreateCube(-0.1f, 0.1f, -0.1f, 0.1f, -0.1f, 0.1f, mVertices, mIndices, true, true, glm::mat4(1.0f));
        spdlog::info("创建立方体: {} 个顶点, {} 个索引", mVertices.size(), mIndices.size());
        //创建网格对象
        mMesh = std::make_shared<XJ::XJMesh>(mVertices, mIndices);

    }

    void OnSceneInit(XJ::XJScene *scene) override
    {

        XJ::XJEntity *kCameraEntity = scene->CreateEntity("CameraEntity");//创建一个实体
        kCameraEntity->AddComponent<XJ::XJCameraComponent>();//添加摄像机组件
        // kCameraEntity->AddComponent<XJ::XJTransformComponent>();  // 添加这行
        mRenderTarget->XJSetCamera(kCameraEntity);//将摄像机实体设置到渲染目标中，以便在渲染过程中使用摄像机信息
        //
        auto kBaseMaterial = XJ::XJMaterialFactory::GetInstance()->CreateMaterial<XJ::XJBaseMaterial>();

        //在这里可以添加场景初始化的代码，例如创建实体、添加组件等
        spdlog::info("场景初始化");
        float x = 0.f;
        for(int i = 0; i < mSmallCubeSize.x; ++i, x += 0.5f)
        {
            float y = 0.f;
            for(int j = 0; j < mSmallCubeSize.y; ++j, y += 0.5f)
            {
                float z = 0.f;
                for(int k = 0; k < mSmallCubeSize.z; ++k, z += 0.5f)
                {
                    XJ::XJEntity* kCube = scene->CreateEntity("CubeEntityA");//创建一个实体
                    auto &kTransformComp = kCube->GetComponent<XJ::XJTransformComponent>();//添加变换组件
                    auto &kMaterialComp = kCube->AddComponent<XJ::XJBaseMaterialComponent>();//添加材质组件
                    kMaterialComp.AddMesh(mMesh.get(), kBaseMaterial);//添加网格组件  并设置

                    kTransformComp.position = glm::vec3(x, y, z);
                    kTransformComp.UpdateModelMatrix(); // 新增：立即更新模型矩阵
                }
            }
        }
    }
    void OnSceneDestroy(XJ::XJScene *scene) override
    {
        //在这里可以添加场景销毁的代码，例如清理资源等
        spdlog::info("场景销毁");
    }

    void OnUpdate(float deltaTime) override
    {
       
        XJ::XJEntity *kCameraEntity = mRenderTarget->XJGetCamera();//获取摄像机实体
        if(kCameraEntity && XJ::XJEntity::HasComponent<XJ::XJCameraComponent>(kCameraEntity))
        {
            mCameraController->UpdateCameraControl(deltaTime, XJGetWindow(), kCameraEntity);
        }
    }
    
    void OnRender() override
    {
        if (XJGetWindow()->IsWindowMinimized())
        {
            // 窗口最小化，跳过渲染，但保留事件处理
            return;
        }   
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

        mRenderTarget->RenderMaterialSystem(kCommandBuffer);//便利系统
        mRenderTarget->EndRenderTarget(kCommandBuffer);//结束渲染通道

     

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
        mMesh.reset();
        mCommandBuffers.clear();
        
        mRenderTarget.reset();
        mRenderPass.reset();
        
        mRender.reset();
        mCameraController.reset();
    }

   
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

    std::shared_ptr<XJ::XJEventTesting>                 mEventTesting;
    std::shared_ptr<XJ::XJEventObserver>                mOvserver;

    // 摄像机控制器
    std::unique_ptr<XJ::XJCameraControllerSystem>       mCameraController;

    VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT; // 多重采样数量

    glm::ivec3 mSmallCubeSize{5 ,5 ,5};//立方体尺寸
   
    

};

XJ::XJApplication* CreateApplicationEntryPoint()
{
    return new XJEngineApp();
}
