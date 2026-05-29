#include "XJEntryPoint.h"
#include "Edit/FileUtil.h"
#include "Edit/Mathinclude.h"
#include "Edit/XJEventTesting.h"
#include "Event/XJWindowEvent.h"

#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanCommandBuffer.h"
#include "Graphic/VulkanCommon.h"
#include "Graphic/XJVulkanDescriptorSet.h"
#include "Graphic/XJVulkanImage.h"

#include "Render/XJRenderTarget.h"
#include "Render/XJRenderer.h"
#include "Render/Resource/XJMaterial.h"
#include "Render/Resource/XJMaterialFactory.h"

#include "Asset/Importer/XJTextureImporter.h"
#include "Render/Resource/XJTextureFactory.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/XJSceneAsset.h"
#include "Asset/Instantiation/XJSceneInstantiator.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include "Asset/Register/XJAssetBootstrap.h"
#include "Asset/XJSceneRuntimeUtil.h"

#include "ECS/System/XJBaseMaterialSystem.h"
#include "ECS/XJScene.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/System/XJCameraControllerSystem.h"
#include "ECS/System/XJUnlitMaterialSystem.h"

#include "UI/XJUIContext.h"
#include "UI/XJEditorRenderer.h"
#include "UI/Viewports/XJScenePreview.h" 
#include "UI/Viewports/XJGamePreview.h"
#include "UI/XJEditorUILayer.h"

#include <iostream>
#include <chrono>
#include <filesystem>



    
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
      
         // 配置 RenderPass 附件
        std::vector<XJ::Attachment> attachments{};
        attachments.resize(2);
        // 颜色附件
        attachments[0].flags = 0;
        attachments[0].format = kSwapchain->XJGetSurfaceInfo().surfaceFormat.format;// 表面格式，颜色附件与交换链格式保持一致
        attachments[0].samples = mSampleCount;// 采样数
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;// 加载时清除
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;// 存储结果
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;// 不关心模板加载
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;// 不关心模板存储
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;// 初始布局未定义
        //attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;// 颜色附件布局
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;// 最终用于显示
        // 深度附件
        attachments[1].flags = 0;
        attachments[1].format = kDevice->XJGetSettings().depthFormat;// 深度格式
        attachments[1].samples = mSampleCount;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;// 深度附件布局
      
        // 子通道
        std::vector<XJ::RenderSubPass> subpasses = // 多重采样配置
        {
            {
                .colorAttachments = { 0 }, // 颜色附件索引
                .depthStencilAttachments = { 1 },// 深度附件索引
                .resolveAttachments = {},   // 解析附件由渲染通道自动添加
                .sampleCount = mSampleCount// 采样数
                // .sampleCount = VK_SAMPLE_COUNT_1_BIT// 采样数
            }
        };

        mRenderPass = std::make_shared<XJ::XJVulkanRenderPass>(kDevice, kPhysicalDevices, attachments, subpasses);// 创建渲染通道对象
        spdlog::info("渲染通道创建成功");
        mRenderTarget = std::make_shared<XJ::XJRenderTarget>(mRenderPass.get());

        mRenderTarget->SetColorClearValue(0, VkClearColorValue{0.1f, 0.1f, 0.1f, 1.0f});// 设置颜色清除值
        mRenderTarget->SetDepthClearValue(VkClearDepthStencilValue{1.0f, 0});// 设置深度清除值
        // 添加材质系统
        mRenderTarget->AddMaterialSystem<XJ::XJBaseMaterialSystem>();
        mRenderTarget->AddMaterialSystem<XJ::XJUnlitMaterialSystem>();

        mRender = std::make_shared<XJ::XJRenderer>();
         // 创建命令池缓冲区
        mCommandBuffers = kDevice->XJGetDefaultCmdPool()->AllocateCommandBuffer(static_cast<uint32_t>(kSwapchain->XJGetSwapchainImages().size()));// 分配命令缓冲区
        spdlog::info("分配了 {} 个命令缓冲区", mCommandBuffers.size());
        // 初始化摄像机控制器：鼠标灵敏度、平移、缩放、旋转
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
        // 初始化纹理和采样器
        XJ::RGBAColor kWhitePixel{255, 255, 255, 255};// 白色像素
        XJ::RGBAColor kBlackPixel{0, 0, 0, 255};// 黑色像素
        XJ::RGBAColor kMultiPixel[4] = { {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}, {255, 255, 0, 255} };// 多像素测试数据
        mWhiteTexture = std::make_shared<XJ::XJTexture>(1,1, &kWhitePixel);// 创建白色纹理
        mBlackTexture = std::make_shared<XJ::XJTexture>(1,1, &kBlackPixel);// 创建黑色纹理
        mMultiPixelTexture = std::make_shared<XJ::XJTexture>(2,2, kMultiPixel);// 创建多像素纹理
        auto kAsset = XJ::XJTextureImporter::ImportTexture(XJ_RES_TEXTURE_DIR"R-C.jpeg");
        if (kAsset) mFileTexture = XJ::XJTextureFactory::CreateTextureFromAsset(*kAsset);// 创建文件纹理
        // 创建默认采样器
        mDefaultSampler = std::make_shared<XJ::XJSampler>(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);// 创建默认采样器

        // 初始化 UI
        XJ::XJEditorRendererInitInfo kUIRendererInfo = {};
        kUIRendererInfo.instance       = kRenderContext->XJGetInstance()->XJGetInstance();
        kUIRendererInfo.physicalDevice = kPhysicalDevices->XJGetPhysicalDevice();
        kUIRendererInfo.device         = kDevice->XJGetDevice();
        kUIRendererInfo.renderPass     = mRenderPass->XJGetRenderPass();
        kUIRendererInfo.commandPool    = kDevice->XJGetDefaultCmdPool()->XJGetCommandPool();
        kUIRendererInfo.queueFamily    = kPhysicalDevices->XJGetGraphicQueueFamilyInfo().queueFamilyIndex;
        kUIRendererInfo.queue          = kDevice->XJGetFirstGraphicQueue()->XJGetQueue();
        kUIRendererInfo.imageCount     = static_cast<uint32_t>(kSwapchain->XJGetSwapchainImages().size());

        mUIContext = std::make_unique<XJ::XJUIContext>();
        mEditorRenderer = std::make_unique<XJ::XJEditorRenderer>();
        mUIContext->Init(static_cast<GLFWwindow*>(XJGetWindow()->XJGetImplWindowPointer()));
        mEditorRenderer->Init(kUIRendererInfo);
        // 初始化场景预览和游戏预览 UI
        // 创建场景预览和游戏预览对象，并完成初始化

        mScenePreview = std::make_unique<XJ::XJScenePreview>();
        mScenePreview->SetViewportName("Scene Preview");
        mScenePreview->Init(kRenderContext);
        mScenePreview->AddMaterialSystem<XJ::XJBaseMaterialSystem>();
        mScenePreview->AddMaterialSystem<XJ::XJUnlitMaterialSystem>();

        mGamePreview = std::make_unique<XJ::XJGamePreview>();
        mGamePreview->SetViewportName("Game Preview"); 
        mGamePreview->Init(kRenderContext);
        mGamePreview->AddMaterialSystem<XJ::XJBaseMaterialSystem>();
        mGamePreview->AddMaterialSystem<XJ::XJUnlitMaterialSystem>();

        mEditorUILayer = std::make_unique<XJ::XJEditorUILayer>(mEditorUIState);
        mEditorUILayer->Init("Resource/Config/EditorUI.json");
    }
    // 场景
    void OnSceneInit(XJ::XJScene *scene) override
    {
        XJ::XJAssetBootstrap bootstrap(mAssetRegistry, kDefaultSceneHandle, kMonkeyMeshHandle);
        bootstrap.LoadOrCreateAssetRegistry();
        mSceneAsset = bootstrap.LoadOrCreateDefaultSceneAsset();
        if (!mSceneAsset)
        {
            spdlog::error("Default scene asset load failed");
            return;
        }

        mSceneInstantiateContext.Registry = &mAssetRegistry;
        mSceneInstantiateContext.SourceScene = { kDefaultSceneHandle, XJ::XJAssetType::Scene };
        mSceneInstantiateContext.DefaultTexture = mWhiteTexture;
        mSceneInstantiateContext.DefaultSampler = mDefaultSampler;
        XJ::XJSceneInstantiator::Instantiate(*mSceneAsset, *scene, &mSceneInstantiateContext);

        XJ::XJEntity* previewCamera = XJ::XJSceneInstantiator::FindInstantiatedEntity(
            mSceneInstantiateContext, XJ::XJUUID(static_cast<uint32_t>(kPreviewCameraEntityId)));
        XJ::XJEntity* gameCamera = XJ::XJSceneRuntimeUtil::FindPrimaryCameraEntity(*scene);

        if (mScenePreview) mScenePreview->SetCamera(previewCamera ? previewCamera : gameCamera);
        if (mGamePreview) mGamePreview->SetCamera(gameCamera ? gameCamera : previewCamera);
        mRenderTarget->XJSetCamera(gameCamera ? gameCamera : previewCamera);

        mEditorUIState.Scene = scene;
        mEditorUIState.AssetRegistry = &mAssetRegistry;
    }
    void OnSceneDestroy(XJ::XJScene *scene) override
    {
        // 可以在这里补充场景销毁时的清理逻辑，例如释放资源等
        spdlog::info("Scene destroyed");
        mEditorUIState.SelectedEntity = nullptr;
        mEditorUIState.Scene = nullptr;
    }
    // UI
    void OnUIBegin() override 
    { 
        if (mUIContext)
        {
            mUIContext->BeginFrame();
        }
    }
    void OnUIEnd() override 
    {
        if (mUIContext)
            mUIContext->EndFrame();
    }
    
    void OnUIDestroy() override
    {
        if (mEditorUILayer)
        {
            mEditorUILayer->SaveConfig();
            mEditorUILayer->Shutdown();
            mEditorUILayer.reset();
        }

        if (mScenePreview) mScenePreview->Shutdown();
        if (mGamePreview) mGamePreview->Shutdown();

        mEditorRenderer.reset();
        mUIContext.reset();
    }

    void OnUpdate(float deltaTime) override
    {
         // ===== UI 开始 =====
        //mUIContext->BeginFrame();
        if (mEditorUILayer)
            mEditorUILayer->DrawUI();

        if (mScenePreview)
            mScenePreview->DrawUI();

        if (mGamePreview)
            mGamePreview->DrawUI();
       
        XJ::XJEntity *kCameraEntity = mRenderTarget->XJGetCamera();// 获取摄像机实体
        if(kCameraEntity && XJ::XJEntity::HasComponent<XJ::XJCameraComponent>(kCameraEntity))
        {
            mCameraController->UpdateCameraControl(deltaTime, XJGetWindow(), kCameraEntity);
        }

         // ===== UI 结束 =====
        //mUIContext->EndFrame();
    }
    
    void OnRender() override
    {
        if (XJGetWindow()->IsWindowMinimized())
        {
            // 窗口最小化时跳过渲染，但保留事件处理
            return;
        }   
        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();
        XJ::XJVulkanSwapchain* kSwapchain = kRenderContext->XJGetSwapchain();

    
        int32_t imageIndex;// 当前图像索引
        if(mRender->XJRendererBegin(&imageIndex, mCommandBuffers))
        {
            mRenderTarget->SetExtent({kSwapchain->XJGetWidth(),kSwapchain->XJGetHeight()});
        }

        if (mScenePreview)
        {
            mScenePreview->PrepareBeforeRender();
        }
        if (mGamePreview)
        {
          mGamePreview->PrepareBeforeRender();
        }
       
        VkCommandBuffer kCommandBuffer = mCommandBuffers[imageIndex];// 取出当前命令缓冲区
        if (kCommandBuffer == VK_NULL_HANDLE) 
        {
            spdlog::error("Command buffer is null");
            return;
        }
        // 开始录制命令缓冲区
        XJ::XJVulkanCommandPool::BeginCommandBuffer(kCommandBuffer);
        // 场景预览离屏渲染
        if (mScenePreview)
        {
            if (!mScenePreview->Render(kCommandBuffer))
            {
                // spdlog::error("ScenePreview render failed");
            }
        }

        if (mGamePreview)
        {
           if (!mGamePreview->Render(kCommandBuffer))
           {
               // spdlog::error("GamePreview render failed");
           }
        }
        
        if (mScenePreview)
        {
           mScenePreview->PostRender();
        }
        if (mGamePreview)
        {
           mGamePreview->PostRender();
        }

        // 正式视图 RT
        if (mRenderTarget->BeginRenderTarget(kCommandBuffer))
        {
            //// 改为由 GLFW 窗口统一处理，不再直接渲染方块：
            // mRenderTarget->RenderMaterialSystem(kCommandBuffer);
            // ===== 新增 UI 渲染 =====
            if (mEditorRenderer && mUIContext)
                mEditorRenderer->RenderDrawData(kCommandBuffer, mUIContext->XJGetDrawData());
            // ===== Viewport Vulkan UI 已绘制完成 =====
            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                GLFWwindow* backup = glfwGetCurrentContext();// 备份当前上下文
        
                ImGui::UpdatePlatformWindows();// 更新平台窗口，处理多视口窗口创建和尺寸变化
                ImGui::RenderPlatformWindowsDefault();// 渲染平台窗口，调用各视口对应的平台渲染逻辑
        
                glfwMakeContextCurrent(backup);// 恢复之前的上下文
            }
             // ===== UI 渲染结束 =====
            mRenderTarget->EndRenderTarget(kCommandBuffer);// 结束渲染通道
        }
        
      
        XJ::XJVulkanCommandPool::EndCommandBuffer(kCommandBuffer);
        // 提交命令缓冲区
        if(mRender->XJRendererEnd(imageIndex, {kCommandBuffer}))
        {
            mRenderTarget->SetExtent({kSwapchain->XJGetWidth(),kSwapchain->XJGetHeight()});
           
        }
    }
    void OnDestroy() override
    {
         // ===== 先关闭 UI =====
        //mEditorRenderer->Shutdown();
        //mUIContext->Shutdown();
        

        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice* kDevice = kRenderContext->XJGetDevice();
        vkDeviceWaitIdle(kDevice->XJGetDevice());// 等待设备空闲
        mWhiteTexture.reset();// 白色纹理
        mBlackTexture.reset();// 黑色纹理
        mMultiPixelTexture.reset();// 多像素纹理
        mFileTexture.reset();// 文件纹理
        mDefaultSampler.reset();// 默认采样器


        mSceneInstantiateContext = {};
        mSceneAsset.reset();
        mCommandBuffers.clear();
        
        mRenderTarget.reset();
        mRenderPass.reset();
        
        mRender.reset();
        mCameraController.reset();
    }

 
   
private:
    // 渲染准备
    std::shared_ptr<XJ::XJVulkanRenderPass>             mRenderPass;
    std::shared_ptr<XJ::XJRenderTarget>                 mRenderTarget;
    std::shared_ptr<XJ::XJRenderer>                     mRender;
    // 渲染资源
    std::vector<VkCommandBuffer>                        mCommandBuffers;
    std::shared_ptr<XJ::XJTexture>                      mWhiteTexture;// 白色纹理
    std::shared_ptr<XJ::XJTexture>                      mBlackTexture;// 黑色纹理
    std::shared_ptr<XJ::XJTexture>                      mMultiPixelTexture;// 多像素纹理
    std::shared_ptr<XJ::XJTexture>                      mFileTexture;// 文件纹理
    std::shared_ptr<XJ::XJSampler>                      mDefaultSampler;// 默认采样器

    // 事件
    std::shared_ptr<XJ::XJEventTesting>                 mEventTesting;
    std::shared_ptr<XJ::XJEventObserver>                mOvserver;
    // UI
    std::unique_ptr<XJ::XJUIContext>                    mUIContext;
    std::unique_ptr<XJ::XJEditorRenderer>               mEditorRenderer;
    std::unique_ptr<XJ::XJScenePreview>                 mScenePreview;
    std::unique_ptr<XJ::XJGamePreview>                  mGamePreview;
    XJ::XJEditorUIState                                 mEditorUIState;
    std::unique_ptr<XJ::XJEditorUILayer>                mEditorUILayer;

    // 摄像机控制器
    std::unique_ptr<XJ::XJCameraControllerSystem>       mCameraController;
    
    VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT; // 多重采样数量

    static constexpr XJ::XJAssetHandle kDefaultSceneHandle = 0x10000001ull;
    static constexpr XJ::XJAssetHandle kMonkeyMeshHandle = 0x20000001ull;
    static constexpr uint64_t kPreviewCameraEntityId = 0x30000001ull;
    static constexpr uint64_t kGameCameraEntityId = 0x30000002ull;
    static constexpr uint64_t kMonkeyEntityId = 0x30000003ull;

    XJ::XJAssetRegistry mAssetRegistry;
    std::shared_ptr<XJ::XJSceneAsset> mSceneAsset;
    XJ::XJSceneInstantiateContext mSceneInstantiateContext;
   
};

XJ::XJApplication* CreateApplicationEntryPoint()
{
    return new XJEngineApp();
}
