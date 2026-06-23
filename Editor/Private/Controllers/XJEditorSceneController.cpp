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

    void XJEditorSceneController::SetCanDeleteEntityCallback(CanDeleteEntityCallback callback)
    {
        mCanDeleteEntityCallback = std::move(callback);
    }
    void XJEditorSceneController::SetShouldExposeEntityCallback(ShouldExposeEntityCallback callback)
    {
        mShouldExposeEntityCallback = std::move(callback);
    }
    void XJEditorSceneController::SetDefaultMeshHandle(XJAssetHandle handle)
    {
        mDefaultMeshHandle = handle;
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
        if (!mScene)
        {
            uiState.SceneView = {};
            uiState.SelectedEntityDetails = {};
            return;
        }
    
        uiState.SceneView = XJEditorSceneService::BuildSceneViewModel(
            *mScene,
            mShouldExposeEntityCallback);
        
        if (uiState.Selection.SelectedEntity != XJ_INVALID_EDITOR_ENTITY_ID)
        {
            if (mShouldExposeEntityCallback &&
                !mShouldExposeEntityCallback(uiState.Selection.SelectedEntity))
            {
                uiState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
                uiState.SelectedEntityDetails = {};
                return;
            }
        
            uiState.SelectedEntityDetails = XJEditorSceneService::BuildEntityDetailsView(
                *mScene,
                uiState.Selection.SelectedEntity,
                mAssetRegistry,
                mShouldExposeEntityCallback);
        }
        else
        {
            uiState.SelectedEntityDetails = {};
        }
    }

    void XJEditorSceneController::ProcessRequests(XJEditorUIState& uiState)
    {
        if(!mScene)
            return;
        //数据更新
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

        if(uiState.SceneRequests.RequestCreateEmptyEntity)
        {
            auto request = uiState.SceneRequests.CreateEmptyEntity;
            uiState.SceneRequests.RequestCreateEmptyEntity = false;
            uiState.SceneRequests.CreateEmptyEntity = {}; 
            
            XJEditorEntityId parentId = request.AsChild ? request.ParentEntity : XJ_INVALID_EDITOR_ENTITY_ID;

            XJEditorEntityId createdId = XJEditorSceneService::CreateEmptyEntity(*mScene, request.Name, parentId);

            if (createdId != XJ_INVALID_EDITOR_ENTITY_ID)
            {
                uiState.Selection.SelectedEntity = createdId;
                uiState.Selection.SelectedAsset = 0;
                uiState.Selection.HighlightedEntities.clear();
                uiState.SelectedEntityDetails = {};
            
                NotifyAfterMutation();
            }
        }

        if(uiState.SceneRequests.RequestAddComponent)//添加组件
        {
            auto request = uiState.SceneRequests.AddComponent;
            uiState.SceneRequests.RequestAddComponent = false;
            uiState.SceneRequests.AddComponent = {};

            bool added = false;

            if (request.ComponentType == XJEditorComponentType::MeshRenderer)
            {
                added = XJEditorSceneService::AddMeshRendererComponent(*mScene, request.EntityId, mDefaultMeshHandle, *mAssetRegistry, mInstantiateContext, mDefaultTexture, mDefaultSampler);
            }
            else
            {
                added = XJEditorSceneService::AddComponent(*mScene, request.EntityId, request.ComponentType);
            }

            if(added)
            {
                uiState.Selection.SelectedEntity = request.EntityId;
                uiState.Selection.SelectedAsset = 0;
                uiState.Selection.HighlightedEntities.clear();
                uiState.SelectedEntityDetails = {};
            
                NotifyAfterMutation();
                RefreshViewModels(uiState);
            }
        }

        if(uiState.SceneRequests.RequestDeleteComponent)//删除组件
        {
            auto request = uiState.SceneRequests.DeleteComponent;
            uiState.SceneRequests.RequestDeleteComponent = false;
            uiState.SceneRequests.DeleteComponent  = {};

            bool deleted = XJEditorSceneService::DeleteComponent(*mScene, request.EntityId, request.ComponentType);

            if(deleted)
            {
                uiState.Selection.SelectedEntity = request.EntityId;
                uiState.Selection.SelectedAsset = 0;
                uiState.Selection.HighlightedEntities.clear();
                uiState.SelectedEntityDetails = {};

                NotifyAfterMutation();
                RefreshViewModels(uiState);
            }
        }

        if (uiState.SceneRequests.RequestSetMeshRendererMesh)//添加mesh
        {
            auto request = uiState.SceneRequests.SetMeshRendererMesh;
            uiState.SceneRequests.RequestSetMeshRendererMesh = false;
            uiState.SceneRequests.SetMeshRendererMesh = {};
        
            bool changed = XJEditorSceneService::SetMeshRendererMesh(*mScene, request.EntityId, request.MeshAsset, *mAssetRegistry, mInstantiateContext, mDefaultTexture, mDefaultSampler);
            
            if (changed)
            {
                uiState.Selection.SelectedEntity = request.EntityId;
                uiState.Selection.SelectedAsset = 0;
                uiState.Selection.HighlightedEntities.clear();
                uiState.SelectedEntityDetails = {};
            
                NotifyAfterMutation();
                RefreshViewModels(uiState);
            }
        }
        if (uiState.SceneRequests.RequestSetMeshRendererMaterial)//设置材质
        {
            //开启设置
            auto request = uiState.SceneRequests.SetMeshRendererMaterial;
            uiState.SceneRequests.RequestSetMeshRendererMaterial = false;
            uiState.SceneRequests.SetMeshRendererMaterial = {};
            
            bool changed = XJEditorSceneService::SetMeshRendererMaterial(*mScene, request.EntityId, request.SlotIndex, request.MaterialAsset, *mAssetRegistry, mInstantiateContext, mDefaultTexture, mDefaultSampler);
            
            if(changed)//更新数据
            {
                uiState.Selection.SelectedEntity = request.EntityId;
                uiState.Selection.SelectedAsset = 0;
                uiState.Selection.HighlightedEntities.clear();
                uiState.SelectedEntityDetails = {};

                NotifyAfterMutation();
                RefreshViewModels(uiState);
            }

        }

        if (uiState.SceneRequests.RequestResetMeshRendererMaterial)//重载材质
        {
            auto request = uiState.SceneRequests.ResetMeshRendererMaterial;
            uiState.SceneRequests.RequestResetMeshRendererMaterial = false;
            uiState.SceneRequests.ResetMeshRendererMaterial = {};
        
            bool changed = XJEditorSceneService::ResetMeshRendererMaterialToDefault(*mScene, request.EntityId, request.SlotIndex, *mAssetRegistry, mInstantiateContext, mDefaultTexture, mDefaultSampler);
            
            if (changed)
            {
                uiState.Selection.SelectedEntity = request.EntityId;
                uiState.Selection.SelectedAsset = 0;
                uiState.Selection.HighlightedEntities.clear();
                uiState.SelectedEntityDetails = {};
            
                NotifyAfterMutation();
                RefreshViewModels(uiState);
            }
        }

        if(uiState.SceneRequests.RequestSetMaterialParameter)//设置材质参数
        {
            auto request = uiState.SceneRequests.SetMaterialParameter;
            uiState.SceneRequests.RequestSetMaterialParameter = false;
            uiState.SceneRequests.SetMaterialParameter = {};

            bool changed = XJEditorSceneService::SetMaterialParameter(
                *mScene,
                request.EntityId,
                request.SlotIndex,
                request.MaterialAsset,
                request.ParameterName,
                request.Value,
                *mAssetRegistry,
                mInstantiateContext,
                mDefaultTexture,
                mDefaultSampler);
            
            if (changed)
            {
                uiState.Selection.SelectedEntity = request.EntityId;
                uiState.Selection.SelectedAsset = 0;
                uiState.Selection.HighlightedEntities.clear();
                uiState.SelectedEntityDetails = {};
            
                NotifyAfterMutation();
                RefreshViewModels(uiState);
            }
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
        //通过ID删除资产
        if (!uiState.SceneRequests.RequestDeleteEntities.empty())
        {
            auto ids = uiState.SceneRequests.RequestDeleteEntities;
            uiState.SceneRequests.RequestDeleteEntities.clear();
            //编辑器摄像机不能删除
            std::vector<XJEditorEntityId> filteredIds;
            for (XJEditorEntityId id : ids)
            {
                if (mCanDeleteEntityCallback && !mCanDeleteEntityCallback(id))
                    continue;
            
                filteredIds.push_back(id);
            }
        
            if (!filteredIds.empty())
            {
                if (mBeforeDeleteCallback)
                    mBeforeDeleteCallback(*mScene, filteredIds);
            
                XJEditorSceneService::DeleteEntities(*mScene, filteredIds);
            
                uiState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
                uiState.Selection.SelectedAsset = 0;
                uiState.Selection.HighlightedEntities.clear();
                uiState.SelectedEntityDetails = {};
            
                NotifyAfterMutation();
            }
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
        //重新命名 更新数据
        uiState.SceneRequests.RenameEntity = {};
        uiState.SceneRequests.UpdateTransform = {};
        uiState.SceneRequests.UpdateCamera = {};

        uiState.SceneRequests.RequestCreateEmptyEntity = false;
        uiState.SceneRequests.CreateEmptyEntity = {};
        //添加组件
        uiState.SceneRequests.RequestAddComponent = false;
        uiState.SceneRequests.AddComponent = {};
        //删除组件
        uiState.SceneRequests.RequestDeleteComponent = false;
        uiState.SceneRequests.DeleteComponent = {};
        //添加mesh
        uiState.SceneRequests.RequestSetMeshRendererMesh = false;
        uiState.SceneRequests.SetMeshRendererMesh = {};
        //设置材质
        uiState.SceneRequests.RequestSetMeshRendererMaterial = false;
        uiState.SceneRequests.SetMeshRendererMaterial = {};
        //重载材质
        uiState.SceneRequests.RequestResetMeshRendererMaterial = false;
        uiState.SceneRequests.ResetMeshRendererMaterial = {};
        //设置材质参数
        uiState.SceneRequests.RequestSetMaterialParameter = false;
        uiState.SceneRequests.SetMaterialParameter = {};
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