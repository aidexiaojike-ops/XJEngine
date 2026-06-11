#include "Controllers/XJEditorSceneController.h"

#include "Asset/Register/XJAssetBootstrap.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include "ECS/XJScene.h"
#include "Services/XJEditorSceneService.h"
#include "UI/XJEditorUIState.h"

#include <spdlog/spdlog.h>
#include <utility>

namespace XJ
{
    void XJEditorSceneController::SetScene(XJScene* scene)
    {
        mScene = scene;
    }

    void XJEditorSceneController::SetAssetRegistry(XJAssetRegistry* registry)
    {
        mAssetRegistry = registry;
    }

    void XJEditorSceneController::SetDefaultResources(std::shared_ptr<XJTexture> defaultTexture, std::shared_ptr<XJSampler> defaultSampler)
    {
        mDefaultTexture = std::move(defaultTexture);
        mDefaultSampler = std::move(defaultSampler);
    }

    void XJEditorSceneController::SetCurrentScenePath(const std::filesystem::path& path)
    {
        mCurrentScenePath = path;
    }

    void XJEditorSceneController::SetBeforeDeleteCallback(BeforeDeleteCallback callback)
    {
        mBeforeDeleteCallback = std::move(callback);
    }

    void XJEditorSceneController::SetAfterMutationCallback(AfterMutationCallback callback)
    {
        mAfterMutationCallback = std::move(callback);
    }

    void XJEditorSceneController::SetBeforeOpenSceneCallback(BeforeOpenSceneCallback callback)
    {
        mBeforeOpenSceneCallback = std::move(callback);
    }

    void XJEditorSceneController::SetAfterOpenSceneCallback(AfterOpenSceneCallback callback)
    {
        mAfterOpenSceneCallback = std::move(callback);
    }
    bool XJEditorSceneController::LoadOrCreateDefaultScene(XJEditorUIState& uiState, XJAssetHandle defaultSceneHandle, 
                                                        XJAssetHandle defaultMeshHandle, const std::filesystem::path& scenePath)
    {
        if (!mScene || !mAssetRegistry)
            return false;

        XJAssetBootstrap bootstrap(*mAssetRegistry, defaultSceneHandle, defaultMeshHandle);
        bootstrap.LoadOrCreateAssetRegistry();
        
        auto sceneAsset  = bootstrap.LoadOrCreateDefaultSceneAsset();//获取默认场景
        if (!sceneAsset)
        {
            spdlog::error("Default scene asset load failed");
            return false;
        }

        mCurrentScenePath = scenePath;
        //实例化场景
        if(!InstantiateSceneAsset(sceneAsset, defaultSceneHandle))
            return false;

        mSceneAsset = sceneAsset;
        mSceneDirty = false;

        uiState.AssetRegistry = mAssetRegistry;
        ResetSelectionForScene(uiState, defaultSceneHandle);
        ResetSceneRequestState(uiState);

        if(mAfterOpenSceneCallback)
            mAfterOpenSceneCallback(*mScene);

        RefreshViewModels(uiState);
        return true;

    }
    bool XJEditorSceneController::OpenSceneAsset(XJEditorUIState& uiState, const std::filesystem::path& scenePath, XJAssetHandle sceneHandle)
    {
        if (!mScene || !mAssetRegistry)
            return false;
        //读取场景
        auto sceneAsset = XJSceneAssetSerializer::LoadFromFile(scenePath);
        if (!sceneAsset)
        {
            spdlog::error("Failed to load scene: {}", scenePath.string());
            return false;
        }
        //重置场景数据
        ResetSelectionForScene(uiState, sceneHandle);
        ResetSceneRequestState(uiState);

        // 打开新场景前先让外部清理相机、viewport 等引用。
        if (mBeforeOpenSceneCallback)
            mBeforeOpenSceneCallback();

        mScene->DestroyAllEntity();

        if (!InstantiateSceneAsset(sceneAsset, sceneHandle))
            return false;

        mSceneAsset = sceneAsset;
        mCurrentScenePath = scenePath;
        mSceneDirty = false;

        if (mAfterOpenSceneCallback)
            mAfterOpenSceneCallback(*mScene);

        RefreshViewModels(uiState);
        return true;  
    }

    void XJEditorSceneController::MarkSceneDirty()
    {
        mSceneDirty = true;
    }

    bool XJEditorSceneController::SaveCurrentScene()
    {
        if (!mScene)
            return false;

        auto sceneAsset = XJSceneAssetSerializer::BuildFromScene(*mScene);
        if(!sceneAsset)
            return false;

        sceneAsset->mHandle = mInstantiateContext.SourceScene.Handle;
        sceneAsset->mName = mCurrentScenePath.stem().string();

        bool saved = XJSceneAssetSerializer::SaveToFile(*sceneAsset, mCurrentScenePath);
        if(saved)
        {
            mSceneAsset = sceneAsset;
            mSceneDirty = false;
        }

        return saved;
    
    }

    void XJEditorSceneController::RefreshViewModels(XJEditorUIState& uiState)
    {
        if(!mScene)
        {
            uiState.SceneView = {};
            uiState.SelectedEntityDetails = {};
            return;
        }

        uiState.SceneView = XJEditorSceneService::BuildSceneViewModel(*mScene);

        if(uiState.Selection.SelectedEntity != XJ_INVALID_EDITOR_ENTITY_ID)
        {
            uiState.SelectedEntityDetails = XJEditorSceneService::BuildEntityDetailsView(*mScene, uiState.Selection.SelectedEntity);
        }
        else
        {
            uiState.SelectedEntityDetails = {}; 
        }
    }

    void XJEditorSceneController::ProcessRequests(XJEditorUIState& uiState)//做一下修改参数
    {
        if(!mScene)
            return;

         if (uiState.SceneRequests.RequestFindEntitiesUsingAsset != 0)
        {
            XJ::XJAssetHandle handle = uiState.SceneRequests.RequestFindEntitiesUsingAsset;

            uiState.SceneRequests.RequestFindEntitiesUsingAsset = 0;

            auto ids = XJ::XJEditorSceneService::FindEntitiesUsingAsset(*mScene, handle);

            uiState.Selection.HighlightedEntities.clear();

            for (auto id : ids)
                uiState.Selection.HighlightedEntities.insert(id);

            uiState.Selection.SelectedAsset = handle;
            uiState.Selection.SelectedEntity = XJ::XJ_INVALID_EDITOR_ENTITY_ID;
        }

        if (uiState.SceneRequests.RequestRenameEntity)
        {
            auto request = uiState.SceneRequests.RenameEntity;
            uiState.SceneRequests.RequestRenameEntity = false;
            uiState.SceneRequests.RenameEntity = {};
        
            XJ::XJEditorSceneService::RenameEntity(*mScene, request.EntityId, request.Name);
        
            NotifyAfterMutation();
        }

        if (uiState.SceneRequests.RequestUpdateTransform)
        {
            auto request = uiState.SceneRequests.UpdateTransform;
            uiState.SceneRequests.RequestUpdateTransform = false;
            uiState.SceneRequests.UpdateTransform = {};

            XJ::XJEditorSceneService::UpdateTransform(*mScene, request);

            NotifyAfterMutation();
        }

        if (uiState.SceneRequests.RequestUpdateCamera)
        {
            auto request = uiState.SceneRequests.UpdateCamera;
            uiState.SceneRequests.RequestUpdateCamera = false;
            uiState.SceneRequests.UpdateCamera = {};

            XJ::XJEditorSceneService::UpdateCamera(*mScene, request);

           NotifyAfterMutation();
        }

        if (!uiState.SceneRequests.RequestDeleteEntities.empty())
        {
            
            auto ids = uiState.SceneRequests.RequestDeleteEntities;
            uiState.SceneRequests.RequestDeleteEntities.clear();

            if (mBeforeDeleteCallback)
                mBeforeDeleteCallback(*mScene, ids);

            XJEditorSceneService::DeleteEntities(*mScene, ids);

            uiState.Selection.SelectedEntity = XJ::XJ_INVALID_EDITOR_ENTITY_ID;
            uiState.Selection.SelectedAsset = 0;
            uiState.Selection.HighlightedEntities.clear();
            uiState.SelectedEntityDetails = {};

            NotifyAfterMutation();
        }

        if (uiState.SceneRequests.RequestSaveScene)
        {
            uiState.SceneRequests.RequestSaveScene = false;
            SaveCurrentScene();
        }
    }

    XJSceneInstantiateContext& XJEditorSceneController::GetInstantiateContext()
    {
        return mInstantiateContext;
    }

    const XJSceneInstantiateContext& XJEditorSceneController::GetInstantiateContext() const
    {
        return mInstantiateContext;
    }

    std::shared_ptr<XJSceneAsset> XJEditorSceneController::GetSceneAsset() const
    {
        return mSceneAsset;
    }

    const std::filesystem::path& XJEditorSceneController::GetCurrentScenePath() const
    {
        return mCurrentScenePath;
    }

    bool XJEditorSceneController::IsSceneDirty() const
    {
        return mSceneDirty;
    }
    void XJEditorSceneController::ResetSceneRequestState(XJEditorUIState& uiState)
    {
        uiState.SceneRequests.RequestDeleteEntities.clear();
        uiState.SceneRequests.RequestFindEntitiesUsingAsset = 0;
        uiState.SceneRequests.RequestSaveScene = false;

        uiState.SceneRequests.RequestRenameEntity = false;
        uiState.SceneRequests.RequestUpdateTransform = false;
        uiState.SceneRequests.RequestUpdateCamera = false;

        uiState.SceneRequests.RenameEntity = {};
        uiState.SceneRequests.UpdateTransform = {};
        uiState.SceneRequests.UpdateCamera = {};
    }

    void XJEditorSceneController::ResetSelectionForScene(XJEditorUIState& uiState, XJAssetHandle sceneHandle)
    {
        uiState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
        uiState.Selection.SelectedAsset = sceneHandle;
        uiState.Selection.HighlightedEntities.clear();//清楚高亮

        uiState.SelectedEntityDetails = {};//删除选择数据
    }
    
    bool XJEditorSceneController::InstantiateSceneAsset(std::shared_ptr<XJSceneAsset> sceneAsset, XJAssetHandle sceneHandle)
    {
        if(!mScene || !sceneAsset || !mAssetRegistry)
            return false;

        mInstantiateContext = {};
        mInstantiateContext.Registry = mAssetRegistry;
        mInstantiateContext.SourceScene = { sceneHandle, XJAssetType::Scene };
        mInstantiateContext.DefaultTexture = mDefaultTexture;
        mInstantiateContext.DefaultSampler = mDefaultSampler;

        XJSceneInstantiator::Instantiate(*sceneAsset, *mScene, &mInstantiateContext);
        return true;
    }

    void XJEditorSceneController::NotifyAfterMutation()
    {
        MarkSceneDirty();
        SaveCurrentScene();

        if (mAfterMutationCallback)
            mAfterMutationCallback();
    }
}