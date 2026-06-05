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
#include "Asset/Loader/XJMeshAssetLoader.h"

#include "ECS/System/XJBaseMaterialSystem.h"
#include "ECS/XJScene.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/System/XJCameraControllerSystem.h"
#include "ECS/System/XJUnlitMaterialSystem.h"

#include "UI/XJUIContext.h"
#include "UI/XJEditorRenderer.h"
#include "UI/Viewports/XJScenePreview.h" 
#include "UI/Viewports/XJGamePreview.h"
#include "UI/XJEditorUILayer.h"
#include "UI/XJEditorDragPayload.h"

#include <iostream>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <cmath>

    
class XJEngineApp : public XJ::XJApplication
{
  
protected:

    void OnExternalFilesDropped(GLFWwindow* window, int count, const char** paths)
    {
        mEditorUIState.PendingExternalDroppedFiles.clear();

        for(int i = 0; i < count; ++i)
        {
            if(paths[i]){
                mEditorUIState.PendingExternalDroppedFiles.emplace_back(paths[i]);
            }
        }

        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        int windowX = 0;
        int windowY = 0;
        glfwGetWindowPos(window, &windowX, &windowY);

        mEditorUIState.PendingExternalDropMousePos = glm::vec2(static_cast<float>(windowX + mouseX), static_cast<float>(windowY + mouseY));

        mEditorUIState.HasPendingExternalDrop = !mEditorUIState.PendingExternalDroppedFiles.empty();
    }

    void  OnConfiguration(XJ::AppSettings *appSettings) override
    {
        // 可以在这里修改默认的应用程序设置，例如窗口标题、初始窗口大小等
        appSettings->windowWidth = 1600;
        appSettings->windowHeight = 1200;
        appSettings->title = "XJEngine Application";
    }
    //临时的碰撞球
    bool IntersectRaySphere(const glm::vec3& rayOrigin,const glm::vec3& rayDir,const glm::vec3& center,float radius,float maxDistance,float& outT)
    {
        glm::vec3 oc = rayOrigin - center;

        float b = glm::dot(oc, rayDir);
        float c = glm::dot(oc, oc) - radius * radius;
        float h = b * b - c;

        if (h < 0.0f)
            return false;

        h = std::sqrt(h);

        float t = -b - h;
        if (t < 0.0f)
            t = -b + h;

        if (t < 0.0f || t > maxDistance)
            return false;

        outT = t;
        return true;
    }
    //射线距离
    bool RaycastSceneWithinDistance(XJ::XJScene* scene, const glm::vec3& rayOrigin, const glm::vec3& rayDir, float maxDistance, glm::vec3& outSpawnPosition)
    {
        if (!scene)
        return false;

        float closestT = maxDistance;
        bool hit = false;

        auto& registry = scene->XJGetEcsRegistry();
        auto view = registry.view<XJ::XJTransformComponent, XJ::XJMeshAssetRefComponent>();

        view.each([&](auto entity, XJ::XJTransformComponent& transform, XJ::XJMeshAssetRefComponent& meshRef)
        {
            float radius = std::max(transform.scale.x, std::max(transform.scale.y, transform.scale.z));
            radius = std::max(radius, 0.5f);

            float t = 0.0f;
            if (!IntersectRaySphere(rayOrigin, rayDir, transform.position, radius, maxDistance, t))
                return;

            if (t < closestT)
            {
                closestT = t;
                hit = true;

                outSpawnPosition = transform.position;
                outSpawnPosition.y += radius + 0.05f;
            }
        });

        return hit;
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
        mOvserver->OnEvent<XJ::XJMouseScrollEvent>([this](const XJ::XJMouseScrollEvent& event)
        {
            XJ::XJEntity* kCameraEntity = mPreviewCameraEntity;
        
            if (mScenePreview && mScenePreview->IsHovered() && kCameraEntity && XJ::XJEntity::HasComponent<XJ::XJCameraComponent>(kCameraEntity))
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
        ///拖拽资产到UI 生成json文件和UUID
        GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(XJGetWindow()->XJGetImplWindowPointer());

        glfwSetWindowUserPointer(glfwWindow, this);
        glfwSetDropCallback(glfwWindow,[](GLFWwindow* window, int count, const char** paths)
        {
            auto* app = static_cast<XJEngineApp*>(glfwGetWindowUserPointer(window));
            if (!app)
                return;

            app->OnExternalFilesDropped(window, count, paths);
        });
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
    XJ::XJEntity* EnsurePreviewCamera(XJ::XJScene* scene)//默认编辑器相机设置
    {
        if(!scene)
            return nullptr;

        for(const auto& [enttEntity, entity] : scene->GetEntities())
        {
            if(entity && entity->XJGetUUID() == XJ::XJUUID(static_cast<uint64_t> (kPreviewCameraEntityId)))
                return entity.get();
        }

        XJ::XJEntity* previewCamera = scene->CreateEntityWithUUID(
                                    XJ::XJUUID(static_cast<uint64_t>(kPreviewCameraEntityId)),
                                    "PreviewCamera");

        auto& transform =  previewCamera->GetComponent<XJ::XJTransformComponent>();
        transform.position = glm::vec3(0.0f, 1.5f, 3.0f);
        transform.rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
        transform.scale = glm::vec3(1.0f);
        transform.UpdateModelMatrix();

        auto& camera = previewCamera->AddComponent<XJ::XJCameraComponent>();
        camera.XJSetFov(65.0f);
        camera.XJSetNear(0.1f);
        camera.XJSetFar(100.0f);

        return previewCamera;
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

        //XJ::XJEntity* previewCamera = XJ::XJSceneInstantiator::FindInstantiatedEntity(
        //    mSceneInstantiateContext, XJ::XJUUID(static_cast<uint32_t>(kPreviewCameraEntityId)));
        //XJ::XJEntity* gameCamera = XJ::XJSceneRuntimeUtil::FindPrimaryCameraEntity(*scene);

        mGameCameraEntity = XJ::XJSceneRuntimeUtil::FindPrimaryCameraEntity(*scene);
        mPreviewCameraEntity = EnsurePreviewCamera(scene);

        if (mScenePreview)
            mScenePreview->SetCamera(mPreviewCameraEntity);

        if (mGamePreview)
            mGamePreview->SetCamera(mGameCameraEntity);

        if (mRenderTarget)
            mRenderTarget->XJSetCamera(mGameCameraEntity);
        

        mEditorUIState.Scene = scene;
        mEditorUIState.AssetRegistry = &mAssetRegistry;

        if(mScenePreview)
        {
            mScenePreview->SetAssetDropCallback([this, scene](const XJ::XJAssetDragPayload& payload)
            {
                CreateEntityFromDroppedAsset(scene, payload);
                MarkSceneDirty();
                SaveCurrentScene();
            });
        }
    }
    void OnSceneDestroy(XJ::XJScene *scene) override
    {
        // 可以在这里补充场景销毁时的清理逻辑，例如释放资源等
        spdlog::info("Scene destroyed");
        mEditorUIState.SelectedEntity = nullptr;
        mEditorUIState.Scene = nullptr;

        mPreviewCameraEntity = nullptr;
        mGameCameraEntity = nullptr;
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
        if (ImGui::GetCurrentContext())//退出的时候保存UI
            ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
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
    void CreateEntityFromDroppedAsset(XJ::XJScene* scene, const XJ::XJAssetDragPayload& payload)// 处理从资源浏览器拖放到场景预览的资产，创建对应的实体
    {
        if (!scene || payload.Type != XJ::XJAssetType::Mesh || payload.Handle == 0)
            return;

        auto metaOpt = mAssetRegistry.GetMeta(payload.Handle);
        if (!metaOpt)
        {
            spdlog::error("Asset meta not found for handle: 0x{:016X}", payload.Handle);
            return;
        }

        XJ::XJEntity* entity = scene->CreateEntity(metaOpt->Name);
        if (!entity)
            return;

        auto& transform = entity->GetComponent<XJ::XJTransformComponent>();
        transform.position = CalculateSpawnPositionFromDropRay(scene, payload);
        transform.UpdateModelMatrix();// 更新模型矩阵以应用位置

        auto& meshRef = entity->AddComponent<XJ::XJMeshAssetRefComponent>();
        meshRef.Mesh = { payload.Handle, XJ::XJAssetType::Mesh };

        XJ::XJMeshAssetLoadContext loadContext;
        loadContext.Registry = &mAssetRegistry;
        loadContext.MeshCache = &mSceneInstantiateContext.MeshCache;

        std::shared_ptr<XJ::XJMesh> gpuMesh = XJ::XJMeshAssetLoader::LoadMesh(payload.Handle, loadContext);

        if(gpuMesh)
        {
            auto& comp = entity->AddComponent<XJ::XJUnlitMaterialComponent>();
            auto mat = XJ::XJMaterialFactory::GetInstance()->CreateDefaultMaterial(
                mWhiteTexture,
                mDefaultSampler);

            if (mat)
                comp.AddMesh(gpuMesh.get(), mat.get());
        } 

        mEditorUIState.SelectedEntity = entity;
        mEditorUIState.SelectedAsset = 0;
    }

    glm::vec3 CalculateSpawnPositionFromDropRay(XJ::XJScene* scene, const XJ::XJAssetDragPayload& payload)//拖拽资产 生成位置
    {
        if (!payload.HasViewportRay)
            return glm::vec3(0.0f);

        glm::vec3 spawnPosition{0.0f};

        if(RaycastSceneWithinDistance(scene, payload.RayOrigin, payload.RayDirection, 5.0f, spawnPosition))
        {
            return spawnPosition;
        }


        return payload.RayOrigin + payload.RayDirection * 5.0f;
    }

    void MarkSceneDirty()//
    {
        mSceneDirty = true;
    }

    bool SaveCurrentScene()
    {
        if(!mEditorUIState.Scene)
            return false;

        auto sceneAsset  =  XJ::XJSceneAssetSerializer::BuildFromScene(*mEditorUIState.Scene);
        if(!sceneAsset)
            return false;

        sceneAsset->mHandle = mSceneInstantiateContext.SourceScene.Handle;
        sceneAsset->mName = mCurrentScenePath.stem().string();

        bool saved = XJ::XJSceneAssetSerializer::SaveToFile(*sceneAsset, mCurrentScenePath);
        if(saved)
        {
            mSceneDirty = false;
        }

        return saved;
    }

    bool OpenSceneAsset(const std::filesystem::path& scenePath, XJ::XJAssetHandle sceneHandle)//打开场景资产
    {
        if(!mEditorUIState.Scene)
            return false;

        auto sceneAsset = XJ::XJSceneAssetSerializer::LoadFromFile(scenePath);//获取路径
        if(!sceneAsset)
        {
            spdlog::error("Failed to load scene: {}", scenePath.string());
            return false;
        }

        mEditorUIState.Scene -> DestroyAllEntity();

        mSceneInstantiateContext = {};
        mSceneInstantiateContext.Registry = &mAssetRegistry;
        mSceneInstantiateContext.SourceScene = { sceneHandle, XJ::XJAssetType::Scene };
        mSceneInstantiateContext.DefaultTexture = mWhiteTexture;
        mSceneInstantiateContext.DefaultSampler = mDefaultSampler;

        XJ::XJSceneInstantiator::Instantiate(*sceneAsset, *mEditorUIState.Scene, &mSceneInstantiateContext);

        //XJ::XJEntity* previewCamera = XJ::XJSceneInstantiator::FindInstantiatedEntity(
        //    mSceneInstantiateContext, XJ::XJUUID(static_cast<uint32_t>(kPreviewCameraEntityId)));
        //XJ::XJEntity* gameCamera = XJ::XJSceneRuntimeUtil::FindPrimaryCameraEntity(*mEditorUIState.Scene);

        mGameCameraEntity = XJ::XJSceneRuntimeUtil::FindPrimaryCameraEntity(*mEditorUIState.Scene); 
        mPreviewCameraEntity = EnsurePreviewCamera(mEditorUIState.Scene);

        if (mScenePreview)
        mScenePreview->SetCamera(mPreviewCameraEntity);

        if (mGamePreview)
            mGamePreview->SetCamera(mGameCameraEntity);

        if (mRenderTarget)
            mRenderTarget->XJSetCamera(mGameCameraEntity);

        mEditorUIState.SelectedEntity = nullptr;    
        mEditorUIState.SelectedAsset = sceneHandle;    

        mSceneAsset = sceneAsset;
        mCurrentScenePath = scenePath;
        mSceneDirty = false;

        return true;
    }

    void OnUpdate(float deltaTime) override
    {
         // ===== UI 开始 =====
        //mUIContext->BeginFrame();
        if (mEditorUILayer)
            mEditorUILayer->DrawUI();

        if(mEditorUIState.RequestOpenScene)
        {
            const std::filesystem::path scenePath = mEditorUIState.RequestedScenePath;
            const XJ::XJAssetHandle sceneHandle = mEditorUIState.RequestedSceneHandle;

            mEditorUIState.RequestOpenScene = false;
            mEditorUIState.RequestedScenePath.clear();
            mEditorUIState.RequestedSceneHandle = 0;

            OpenSceneAsset(scenePath, sceneHandle);
        }

        if (mScenePreview)
            mScenePreview->DrawUI();

        if (mGamePreview)
            mGamePreview->DrawUI();
       
        XJ::XJEntity *kCameraEntity = mPreviewCameraEntity;// 获取摄像机实体
        if(mScenePreview && mScenePreview->ShouldControlCamera() && kCameraEntity && XJ::XJEntity::HasComponent<XJ::XJCameraComponent>(kCameraEntity))
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
    
    bool mSceneDirty = false;
    std::filesystem::path mCurrentScenePath = "Resource/Scenes/Default.xjscene";

    XJ::XJEntity* mPreviewCameraEntity = nullptr;
    XJ::XJEntity* mGameCameraEntity = nullptr;
   
};

XJ::XJApplication* CreateApplicationEntryPoint()
{
    return new XJEngineApp();
}
