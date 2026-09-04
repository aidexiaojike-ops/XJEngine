#include "UI/Viewports/XJEditorViewportSystem.h"

#include "Controllers/XJEditorCameraController.h"
#include "Controllers/XJEditorCameraManager.h"
#include "ECS/XJReservedUUID.h"
#include "ECS/XJScene.h"
#include "Edit/XJGlfwWindow.h"
#include "Render/XJEditorRenderResources.h"
#include "Render/System/XJBaseMaterialSystem.h"
#include "Render/System/XJUnlitMaterialSystem.h"
#include "UI/Viewports/XJGamePreview.h"
#include "UI/Viewports/XJScenePreview.h"

#include <spdlog/spdlog.h>

namespace XJ
{
    class XJEditorViewportSystem::Impl
    {
    public:
        XJGlfwWindow* Window = nullptr;
        XJRenderContext* RenderContext = nullptr;
        XJRenderTarget* MainRenderTarget = nullptr;
        XJEditorRenderResources* Resources = nullptr;
        XJScene* Scene = nullptr;

        std::unique_ptr<XJScenePreview> ScenePreview;
        std::unique_ptr<XJGamePreview> GamePreview;
        std::unique_ptr<XJEditorCameraController>
            CameraController;

        XJEditorCameraManager CameraManager;

        bool Initialized = false;
    };

    XJEditorViewportSystem::XJEditorViewportSystem()
        : mImpl(std::make_unique<Impl>())
    {
    }

    XJEditorViewportSystem::~XJEditorViewportSystem()
    {
        Shutdown();
    }

    //初始化
    bool XJEditorViewportSystem::Init(const XJEditorViewportSystemInitInfo& info)
    {
        Shutdown();

        if (!info.Window ||
            !info.RenderContext ||
            !info.MainRenderTarget ||
            !info.Resources ||
            !info.Resources->IsInitialized())
        {
            spdlog::error(
                "Viewport system initialization failed: "
                "required service is unavailable.");
            return false;
        }

        mImpl->Window = info.Window;
        mImpl->RenderContext = info.RenderContext;
        mImpl->MainRenderTarget = info.MainRenderTarget;
        mImpl->Resources = info.Resources;

        mImpl->ScenePreview =
            std::make_unique<XJScenePreview>();
        mImpl->ScenePreview->SetViewportName(
            "Scene Preview");

        if (!mImpl->ScenePreview->Init(info.RenderContext))
        {
            spdlog::error(
                "Scene Preview initialization failed.");
            Shutdown();
            return false;
        }

        mImpl->ScenePreview->AddMaterialSystem<XJBaseMaterialSystem>();
        mImpl->ScenePreview->AddMaterialSystem<XJUnlitMaterialSystem>();

        mImpl->GamePreview =
            std::make_unique<XJGamePreview>();
        mImpl->GamePreview->SetViewportName(
            "Game Preview");

        if (!mImpl->GamePreview->Init(info.RenderContext))
        {
            spdlog::error(
                "Game Preview initialization failed.");
            Shutdown();
            return false;
        }

        mImpl->GamePreview->AddMaterialSystem<XJBaseMaterialSystem>();
        mImpl->GamePreview->AddMaterialSystem<XJUnlitMaterialSystem>();

        mImpl->CameraController =
            std::make_unique<XJEditorCameraController>(
                0.25f,
                0.05f,
                0.3f,
                0.25f);

        mImpl->CameraManager.BindViewports(
            mImpl->ScenePreview.get(),
            mImpl->GamePreview.get(),
            mImpl->MainRenderTarget);

        mImpl->CameraManager.BindCameraController(
            mImpl->CameraController.get());

        mImpl->Initialized = true;
        return true;
    }

    //场景与每帧更新
    bool XJEditorViewportSystem::AttachScene(XJScene& scene)
    {
        if (!mImpl->Initialized || mImpl->Scene)
            return false;

        mImpl->Scene = &scene;
            // 编辑态：两个视口都渲染编辑器场景；Play 时 GamePreview 再切到运行时克隆。
        mImpl->ScenePreview->SetScene(&scene);
        mImpl->GamePreview ->SetScene(&scene);

        mImpl->CameraManager.SetupCamerasForScene(
            &scene,
            XJ_PREVIEW_CAMERA_UUID);

        return true;
    }

    void XJEditorViewportSystem::DetachScene(XJScene* scene)
    {
        if (!mImpl->Scene)
            return;

        if (scene && scene != mImpl->Scene)
            return;

        mImpl->CameraManager.ClearAllCameraReferences();

        // 清空注入的场景指针，避免悬垂。
        mImpl->ScenePreview->SetScene(nullptr);
        mImpl->GamePreview->SetScene(nullptr);
        mImpl->Scene = nullptr;
    }

    void XJEditorViewportSystem::DrawUI()
    {
        if (!mImpl->Initialized)
            return;

        if (mImpl->ScenePreview)
            mImpl->ScenePreview->DrawUI();

        if (mImpl->GamePreview)
            mImpl->GamePreview->DrawUI();
    }

    void XJEditorViewportSystem::Update(float deltaTime)
    {
        if (!mImpl->Initialized)
            return;

        mImpl->CameraManager.ValidateCameraPointers();

        mImpl->CameraManager.UpdatePreviewCameraControl(
            deltaTime,
            mImpl->Window);
    }

    void XJEditorViewportSystem::OnMouseScroll(float yOffset)
    {
        if (mImpl->Initialized)
            mImpl->CameraManager.OnMouseScroll(yOffset);
    }
    //Getter 与 Shutdown
    XJScenePreview* XJEditorViewportSystem::GetScenePreview() const
    {
        return mImpl->ScenePreview.get();
    }

    XJGamePreview* XJEditorViewportSystem::GetGamePreview() const
    {
        return mImpl->GamePreview.get();
    }

    void XJEditorViewportSystem::Shutdown()
    {
        if (!mImpl)
            return;

        DetachScene(nullptr);

        mImpl->CameraManager.ClearAllCameraReferences();
        mImpl->CameraManager.BindViewports(
            nullptr, nullptr, nullptr);
        mImpl->CameraManager.BindCameraController(nullptr);

        if (mImpl->ScenePreview)
        {
            mImpl->ScenePreview->SetAssetDropCallback({});
            mImpl->ScenePreview->Shutdown();
            mImpl->ScenePreview.reset();
        }

        if (mImpl->GamePreview)
        {
            mImpl->GamePreview->Shutdown();
            mImpl->GamePreview.reset();
        }

        mImpl->CameraController.reset();

        mImpl->Window = nullptr;
        mImpl->RenderContext = nullptr;
        mImpl->MainRenderTarget = nullptr;
        mImpl->Resources = nullptr;
        mImpl->Initialized = false;
    }

    void XJEditorViewportSystem::BeforeDeleteEntities(XJScene& scene, const std::vector<XJEditorEntityId>& ids)
    {
        if (mImpl->Initialized)
            mImpl->CameraManager.ClearIfDeleted(scene, ids);
    }

    void XJEditorViewportSystem::BeforeOpenScene()
    {
        if (mImpl->Initialized)
            mImpl->CameraManager.ClearAllCameraReferences();
    }

    void XJEditorViewportSystem::AfterOpenScene(XJScene& scene)
    {
        if (!mImpl->Initialized)
            return;

        mImpl->Scene = &scene;

        mImpl->ScenePreview->SetScene(&scene);
        mImpl->GamePreview->SetScene(&scene);

        mImpl->CameraManager.SetupCamerasForScene(
            &scene,
            XJ_PREVIEW_CAMERA_UUID);
    }

    void XJEditorViewportSystem::RefreshSceneCameras()
    {
        if (!mImpl->Initialized || !mImpl->Scene)
            return;

        mImpl->CameraManager.SetupCamerasForScene(
            mImpl->Scene,
            XJ_PREVIEW_CAMERA_UUID);
    }

    bool XJEditorViewportSystem::IsProtectedEditorEntity(XJEditorEntityId id) const
    {
        return mImpl->Initialized && mImpl->CameraManager.IsProtectedEditorCamera(id);
    }

    void XJEditorViewportSystem::SetPlayState(XJEditorPlayState state)
    {
        if (mImpl->GamePreview)
            mImpl->GamePreview->SetPlayState(state);
    }

    void XJEditorViewportSystem::BeginPlay(XJScene* runtimeScene, XJEntity* runtimeCamera)
    {
        if (!mImpl->Initialized || !mImpl->GamePreview)
            return;

        mImpl->GamePreview->SetScene(runtimeScene);
        mImpl->GamePreview->SetCamera(runtimeCamera);
    }

    void XJEditorViewportSystem::EndPlay()
    {
        if (!mImpl->Initialized || !mImpl->GamePreview)
            return;

        // 恢复编辑器场景与编辑器主相机
        mImpl->GamePreview->SetScene(mImpl->Scene);
        mImpl->GamePreview->SetCamera(mImpl->CameraManager.GetGameCamera());
    }
}
