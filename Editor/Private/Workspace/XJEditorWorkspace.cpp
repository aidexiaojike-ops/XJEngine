#include "Workspace/XJEditorWorkspace.h"
#include "Render/Resource/XJMaterialFactory.h"

#include "Asset/XJAssetRegistry.h"
#include "Controllers/XJEditorAssetController.h"
#include "Controllers/XJEditorSceneController.h"
#include "Controllers/XJEditorSceneAssetDropController.h"
#include "UI/XJEditorDragPayload.h"
#include "UI/XJEditorUIState.h"
#include "ECS/XJScene.h"
#include "Services/XJEditorAssetService.h"

#include <spdlog/spdlog.h>
#include <utility>

namespace XJ
{
    class XJEditorWorkspace::Impl
    {
        public:
            XJScene* Scene = nullptr;
        
            // UIState 必须先于持有其引用的 UI Layer 创建，
            // 并晚于 UI Layer 销毁。
            XJEditorUIState UIState;
            XJAssetRegistry AssetRegistry;
        
            XJEditorAssetController AssetController;
            XJEditorSceneController SceneController;
            XJEditorSceneAssetDropController SceneAssetDropController;

            std::shared_ptr<XJTexture> DefaultTexture;
            std::shared_ptr<XJSampler> DefaultSampler;
            
            std::filesystem::path ResourceRoot;
            std::filesystem::path RegistryPath;
        
            bool Initialized = false;
            std::filesystem::path DefaultScenePath;

            XJAssetHandle DefaultSceneHandle = 0;
            XJAssetHandle InitialSceneMeshHandle = 0;
            XJAssetHandle DefaultComponentMeshHandle = 0;
    };

    XJEditorWorkspace::XJEditorWorkspace()
        : mImpl(std::make_unique<Impl>())
    {
    }

    XJEditorWorkspace::~XJEditorWorkspace()
    {
        Shutdown();
    }

    bool XJEditorWorkspace::Init( const XJEditorWorkspaceInitInfo& info)
    {
        Shutdown();

        if (info.ResourceRoot.empty() ||
            info.RegistryPath.empty() ||
            info.DefaultScenePath.empty() ||
            info.DefaultSceneHandle == 0 ||
            info.InitialSceneMeshHandle == 0 ||
            info.DefaultComponentMeshHandle == 0)
        {
            spdlog::error(
                "Editor workspace configuration is invalid.");
            return false;
        }

        mImpl->ResourceRoot = info.ResourceRoot;
        mImpl->RegistryPath = info.RegistryPath;
        mImpl->DefaultScenePath = info.DefaultScenePath;
        mImpl->DefaultSceneHandle = info.DefaultSceneHandle;
        mImpl->InitialSceneMeshHandle = info.InitialSceneMeshHandle;
        mImpl->DefaultComponentMeshHandle = info.DefaultComponentMeshHandle;

        // 优先读取持久化 registry；不存在时扫描 Resource 并创建。
        if (!mImpl->AssetRegistry.Load(mImpl->RegistryPath))
        {
            if (!XJEditorAssetService::RefreshRegistry(
                    mImpl->AssetRegistry,
                    mImpl->ResourceRoot,
                    mImpl->RegistryPath))
            {
                spdlog::error("Editor workspace failed to initialize asset registry.");
                return false;
            }
        }

        mImpl->UIState.AssetRegistry = &mImpl->AssetRegistry;

        XJMaterialFactory::GetInstance()->SetAssetRegistry(&mImpl->AssetRegistry);
        mImpl->AssetController.SetAssetRegistry(&mImpl->AssetRegistry);
        mImpl->AssetController.SetRegistryPath(mImpl->RegistryPath);
        mImpl->AssetController.SetRootPath(mImpl->ResourceRoot);
        
        mImpl->SceneController.SetAssetRegistry(&mImpl->AssetRegistry);
        
        mImpl->Initialized = true;

        return true;
    }

    bool XJEditorWorkspace::AttachScene(XJScene& scene)//加载默认场景
    {
        if (!mImpl->Initialized || mImpl->Scene)
            return false;

        mImpl->Scene = &scene;

        mImpl->AssetController.SetScene(&scene);
        mImpl->SceneController.SetScene(&scene);

        mImpl->SceneController.SetCurrentScenePath(
            mImpl->DefaultScenePath);

        mImpl->SceneController.SetDefaultMeshHandle(
            mImpl->DefaultComponentMeshHandle);

        if (!mImpl->SceneController.LoadOrCreateDefaultScene(
                mImpl->UIState,
                mImpl->DefaultSceneHandle,
                mImpl->InitialSceneMeshHandle,
                mImpl->DefaultScenePath))
        {
            spdlog::error(
                "Editor default scene initialization failed.");

            mImpl->AssetController.SetScene(nullptr);
            mImpl->SceneController.SetScene(nullptr);
            mImpl->Scene = nullptr;
            return false;
        }

        return true;
    }

    void XJEditorWorkspace::DetachScene(XJScene* scene)
    {
        if (!mImpl->Scene)
            return;

        if (scene && scene != mImpl->Scene)
            return;

        mImpl->AssetController.SetScene(nullptr);
        mImpl->SceneController.SetScene(nullptr);
        // 普通 scene detach 保留默认资源和 hooks，允许之后重新 AttachScene。
        mImpl->SceneController.ClearSceneReferences();

        mImpl->UIState.Selection = {};
        mImpl->UIState.SceneRequests = {};
        mImpl->UIState.SceneView = {};
        mImpl->UIState.SelectedEntityDetails = {};

        mImpl->Scene = nullptr;
    }

    void XJEditorWorkspace::Update()
    {
        if (!mImpl->Initialized || !mImpl->Scene)
            return;

        if (mImpl->UIState.SceneRequests.RequestOpenScene)
        {
           const std::filesystem::path scenePath =
               mImpl->UIState.SceneRequests
                   .RequestedScenePath;
        
           const XJAssetHandle sceneHandle =
               mImpl->UIState.SceneRequests
                   .RequestedSceneHandle;
        
           mImpl->UIState.SceneRequests.RequestOpenScene = false;
           mImpl->UIState.SceneRequests.RequestedScenePath.clear();
           mImpl->UIState.SceneRequests.RequestedSceneHandle = 0;
        
           mImpl->SceneController.OpenSceneAsset(
               mImpl->UIState,
               scenePath,
               sceneHandle);
        }

        mImpl->AssetController.ProcessRequests(
            mImpl->UIState);

        mImpl->SceneController.ProcessRequests(
            mImpl->UIState);

        // 所有请求完成后统一创建一次 UI 快照。
        mImpl->SceneController.RefreshViewModels(
            mImpl->UIState);
    }

    XJEditorUIState& XJEditorWorkspace::GetUIState()
    {
        return mImpl->UIState;
    }

    const XJEditorUIState&
    XJEditorWorkspace::GetUIState() const
    {
        return mImpl->UIState;
    }

    void XJEditorWorkspace::Shutdown()
    {
        
        
        ClearSceneHooks();
        
        DetachScene(nullptr);
        
        if (!mImpl)
            return;
        mImpl->SceneController.SetDefaultResources(nullptr, nullptr);
        mImpl->UIState.AssetRegistry = nullptr;
        mImpl->ResourceRoot.clear();
        mImpl->RegistryPath.clear();
        mImpl->DefaultScenePath.clear();
        mImpl->DefaultSceneHandle = 0;
        mImpl->InitialSceneMeshHandle = 0;
        mImpl->DefaultComponentMeshHandle = 0;
        mImpl->Initialized = false;
        mImpl->DefaultSampler.reset();
        mImpl->DefaultTexture.reset();
    }

    void XJEditorWorkspace::SetDefaultResources(std::shared_ptr<XJTexture> texture, std::shared_ptr<XJSampler> sampler)
    {
        if (!mImpl)
            return;

        mImpl->DefaultTexture = std::move(texture);
        mImpl->DefaultSampler = std::move(sampler);

        mImpl->SceneController.SetDefaultResources(mImpl->DefaultTexture, mImpl->DefaultSampler);
    }

    void XJEditorWorkspace::SetSceneHooks(XJEditorWorkspaceSceneHooks hooks)
    {
        mImpl->SceneController.SetBeforeDeleteCallback(
            std::move(hooks.BeforeDelete));

        mImpl->SceneController.SetBeforeOpenSceneCallback(
            std::move(hooks.BeforeOpen));

        mImpl->SceneController.SetAfterOpenSceneCallback(
            std::move(hooks.AfterOpen));

        mImpl->SceneController.SetAfterMutationCallback(
            std::move(hooks.AfterMutation));

        mImpl->SceneController.SetCanDeleteEntityCallback(
            std::move(hooks.CanDeleteEntity));

        mImpl->SceneController.SetShouldExposeEntityCallback(
            std::move(hooks.ShouldExposeEntity));
    }

    void XJEditorWorkspace::ClearSceneHooks()
    {
        mImpl->SceneController.SetBeforeDeleteCallback({});
        mImpl->SceneController.SetBeforeOpenSceneCallback({});
        mImpl->SceneController.SetAfterOpenSceneCallback({});
        mImpl->SceneController.SetAfterMutationCallback({});
        mImpl->SceneController.SetCanDeleteEntityCallback({});
        mImpl->SceneController.SetShouldExposeEntityCallback({});
    }
    bool XJEditorWorkspace::HandleSceneAssetDrop(const XJAssetDragPayload& payload)
    {
        if (!mImpl->Initialized || !mImpl->Scene || !mImpl->DefaultTexture || !mImpl->DefaultSampler)
        {
            return false;
        }

        // 视口拖放不经过 SceneRequests，仍要通过 SceneController
        // 的外部修改入口记录 Undo/Redo 快照。
        return mImpl->SceneController.ExecuteExternalMutation(
            mImpl->UIState,
            [this, &payload]()
            {
                return mImpl->SceneAssetDropController
                    .CreateEntityFromDroppedAsset(
                        *mImpl->Scene,
                        payload,
                        mImpl->AssetRegistry,
                        mImpl->SceneController
                            .GetInstantiateContext(),
                        mImpl->UIState,
                        mImpl->DefaultTexture,
                        mImpl->DefaultSampler);
            });
    }

}
