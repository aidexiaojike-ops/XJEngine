#include "Controllers/XJEditorSceneController.h"

#include "Asset/Importer/XJMaterialImporter.h"
#include "Asset/Register/XJAssetBootstrap.h"
#include "Asset/Serialization/XJMaterialAssetSerializer.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include "Asset/XJAssetRegistry.h"
#include "ECS/XJScene.h"
#include "Render/Resource/XJMaterialFactory.h"
#include "Services/XJEditorSceneService.h"
#include "UI/XJEditorUIState.h"

#include <algorithm>
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

        static constexpr XJAssetHandle TJCubeMeshHandle = 0x20000002ull;//来自代码的cube
        XJAssetBootstrap bootstrap(*mAssetRegistry, defaultSceneHandle, defaultMeshHandle, TJCubeMeshHandle);
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

        ClearHistory();
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

        // Snapshot current scene before InstantiateSceneAsset mutates mScene.
        // XJSceneInstantiator::Instantiate clears the target scene internally.
        std::shared_ptr<XJSceneAsset> previousSceneAsset =
            XJSceneAssetSerializer::BuildFromScene(*mScene);

        const std::filesystem::path previousScenePath = mCurrentScenePath;
        const bool previousSceneDirty = mSceneDirty;
        const XJAssetHandle previousSceneHandle =
            mInstantiateContext.SourceScene.Handle != 0
                ? mInstantiateContext.SourceScene.Handle
                : (mSceneAsset ? mSceneAsset->mHandle : 0);
        // 打开新场景前先让外部清理相机、viewport 等引用。
        if (mBeforeOpenSceneCallback)
            mBeforeOpenSceneCallback();

        if (!InstantiateSceneAsset(sceneAsset, sceneHandle))
        {
            spdlog::error("Failed to instantiate scene: {}", scenePath.string());

            if (previousSceneAsset)
            {
                // Roll back to the scene that was visible before the failed open.
                if (!InstantiateSceneAsset(previousSceneAsset, previousSceneHandle))
                {
                    spdlog::error("Failed to roll back previous scene after open failure.");
                }
            }

            mSceneAsset = previousSceneAsset;
            mCurrentScenePath = previousScenePath;
            mSceneDirty = previousSceneDirty;

            if (mAfterOpenSceneCallback && mScene)
                mAfterOpenSceneCallback(*mScene);

            RefreshViewModels(uiState);
            return false;
        }
        mSceneAsset = sceneAsset;
        mCurrentScenePath = scenePath;
        mSceneDirty = false;

        //重置场景数据
        ResetSelectionForScene(uiState, sceneHandle);
        ResetSceneRequestState(uiState);

    
        if (mAfterOpenSceneCallback)
            mAfterOpenSceneCallback(*mScene);

        ClearHistory();
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

    XJEditorSceneController::SceneHistorySnapshot XJEditorSceneController::CaptureHistorySnapshot(
        const XJEditorUIState& uiState,
        const std::vector<XJAssetHandle>& materialHandles) const
    {
        SceneHistorySnapshot snapshot;
        snapshot.Selection = uiState.Selection;

        if (mScene)
            snapshot.Scene = XJSceneAssetSerializer::BuildFromScene(*mScene);

        if (!mAssetRegistry)
            return snapshot;

        for (XJAssetHandle handle : materialHandles)
        {
            if (handle == 0)
                continue;

            const auto duplicate = std::find_if(
                snapshot.Materials.begin(),
                snapshot.Materials.end(),
                [handle](const MaterialHistorySnapshot& item)
                {
                    return item.Handle == handle;
                });
            if (duplicate != snapshot.Materials.end())
                continue;

            auto meta = mAssetRegistry->GetMeta(handle);
            if (!meta || meta->Type != XJAssetType::Material)
                continue;

            auto material = XJMaterialImporter::ImportMaterial(meta->SourcePath.string());
            if (!material)
                continue;

            material->mHandle = handle;
            material->mName = meta->Name;
            material->mPath = meta->SourcePath;
            snapshot.Materials.push_back({handle, meta->SourcePath, std::move(material)});
        }

        return snapshot;
    }

    bool XJEditorSceneController::RestoreHistorySnapshot(
        const SceneHistorySnapshot& snapshot,
        XJEditorUIState& uiState)
    {
        if (!mScene || !snapshot.Scene)
            return false;

        // 材质参数存放在独立 .xjmat 文件中，必须先恢复文件，再重建场景运行时材质。
        for (const auto& materialSnapshot : snapshot.Materials)
        {
            if (!materialSnapshot.Asset || materialSnapshot.Path.empty())
                continue;

            if (!XJMaterialAssetSerializer::SaveToFile(*materialSnapshot.Asset, materialSnapshot.Path))
                return false;

            XJMaterialFactory::GetInstance()->InvalidateMaterialAsset(materialSnapshot.Handle);
            XJEditorSceneService::InvalidateMaterialInspectorCache(materialSnapshot.Handle);
        }

        const XJAssetHandle sceneHandle = mInstantiateContext.SourceScene.Handle;
        if (!InstantiateSceneAsset(snapshot.Scene, sceneHandle))
            return false;

        uiState.Selection = snapshot.Selection;
        uiState.SelectedEntityDetails = {};

        // 预览相机不会写入场景快照，恢复后通过现有回调重新创建并绑定视口。
        if (mAfterOpenSceneCallback)
            mAfterOpenSceneCallback(*mScene);

        MarkSceneDirty();
        RefreshViewModels(uiState);
        return true;
    }

    void XJEditorSceneController::PushUndoSnapshot(SceneHistorySnapshot snapshot)
    {
        if (!snapshot.Scene)
            return;

        mUndoStack.push_back(std::move(snapshot));
        while (mUndoStack.size() > kMaxHistoryEntries)
            mUndoStack.pop_front();

        // 新修改会形成新的历史分支，之前的 Redo 链必须失效。
        mRedoStack.clear();
    }

    bool XJEditorSceneController::CanUndo() const
    {
        return !mUndoStack.empty();
    }

    bool XJEditorSceneController::CanRedo() const
    {
        return !mRedoStack.empty();
    }

    bool XJEditorSceneController::Undo(XJEditorUIState& uiState)
    {
        if (!CanUndo())
            return false;

        const SceneHistorySnapshot& target = mUndoStack.back();
        std::vector<XJAssetHandle> materialHandles;
        materialHandles.reserve(target.Materials.size());
        for (const auto& item : target.Materials)
            materialHandles.push_back(item.Handle);

        SceneHistorySnapshot current = CaptureHistorySnapshot(uiState, materialHandles);
        SceneHistorySnapshot snapshot = std::move(mUndoStack.back());
        mUndoStack.pop_back();

        if (!RestoreHistorySnapshot(snapshot, uiState))
        {
            mUndoStack.push_back(std::move(snapshot));
            return false;
        }

        mRedoStack.push_back(std::move(current));
        while (mRedoStack.size() > kMaxHistoryEntries)
            mRedoStack.pop_front();
        return true;
    }

    bool XJEditorSceneController::Redo(XJEditorUIState& uiState)
    {
        if (!CanRedo())
            return false;

        const SceneHistorySnapshot& target = mRedoStack.back();
        std::vector<XJAssetHandle> materialHandles;
        materialHandles.reserve(target.Materials.size());
        for (const auto& item : target.Materials)
            materialHandles.push_back(item.Handle);

        SceneHistorySnapshot current = CaptureHistorySnapshot(uiState, materialHandles);
        SceneHistorySnapshot snapshot = std::move(mRedoStack.back());
        mRedoStack.pop_back();

        if (!RestoreHistorySnapshot(snapshot, uiState))
        {
            mRedoStack.push_back(std::move(snapshot));
            return false;
        }

        mUndoStack.push_back(std::move(current));
        while (mUndoStack.size() > kMaxHistoryEntries)
            mUndoStack.pop_front();
        return true;
    }

    void XJEditorSceneController::ClearHistory()
    {
        mUndoStack.clear();
        mRedoStack.clear();
    }

    bool XJEditorSceneController::ExecuteExternalMutation(
        XJEditorUIState& uiState,
        const std::function<bool()>& action)
    {
        if (!mScene || !action)
            return false;

        // 视口拖放等操作不经过 SceneRequests，也必须先保存修改前快照。
        SceneHistorySnapshot before = CaptureHistorySnapshot(uiState);
        if (!before.Scene || !action())
            return false;

        PushUndoSnapshot(std::move(before));
        NotifyAfterMutation();
        return true;
    }

    void XJEditorSceneController::ProcessRequests(XJEditorUIState& uiState)
    {
        if(!mScene)
            return;

        // Undo/Redo 会整体重建场景，必须优先处理，并丢弃本帧基于旧快照产生的其他请求。
        if (uiState.SceneRequests.RequestUndo)
        {
            uiState.SceneRequests.RequestUndo = false;
            Undo(uiState);
            ResetSceneRequestState(uiState);
            return;
        }

        if (uiState.SceneRequests.RequestRedo)
        {
            uiState.SceneRequests.RequestRedo = false;
            Redo(uiState);
            ResetSceneRequestState(uiState);
            return;
        }

        const bool hasMutationRequest =
            uiState.SceneRequests.RequestCreateEmptyEntity ||
            uiState.SceneRequests.RequestAddComponent ||
            uiState.SceneRequests.RequestDeleteComponent ||
            uiState.SceneRequests.RequestSetMeshRendererMesh ||
            uiState.SceneRequests.RequestSetMeshRendererMaterial ||
            uiState.SceneRequests.RequestResetMeshRendererMaterial ||
            uiState.SceneRequests.RequestSetMaterialParameter ||
            uiState.SceneRequests.RequestResetMaterialParameter ||
            uiState.SceneRequests.RequestRenameEntity ||
            uiState.SceneRequests.RequestUpdateTransform ||
            uiState.SceneRequests.RequestUpdateCamera ||
            !uiState.SceneRequests.RequestDeleteEntities.empty();

        std::vector<XJAssetHandle> changedMaterialHandles;
        if (uiState.SceneRequests.RequestSetMaterialParameter)
            changedMaterialHandles.push_back(uiState.SceneRequests.SetMaterialParameter.MaterialAsset);
        if (uiState.SceneRequests.RequestResetMaterialParameter)
            changedMaterialHandles.push_back(uiState.SceneRequests.ResetMaterialParameter.MaterialAsset);

        // 同一帧的多个请求作为一个事务撤销。空闲帧不构建快照，避免 Undo 功能
        // 重新引入每帧序列化整个场景的性能问题。
        SceneHistorySnapshot before;
        if (hasMutationRequest)
            before = CaptureHistorySnapshot(uiState, changedMaterialHandles);
        mHistoryMutationOccurred = false;

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
            }
        }

        if (uiState.SceneRequests.RequestResetMaterialParameter)
        {
            auto request = uiState.SceneRequests.ResetMaterialParameter;
            uiState.SceneRequests.RequestResetMaterialParameter = false;
            uiState.SceneRequests.ResetMaterialParameter = {};

            bool changed = XJEditorSceneService::ResetMaterialParameter(
                *mScene,
                request.EntityId,
                request.SlotIndex,
                request.MaterialAsset,
                request.ParameterName,
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

        if (mHistoryMutationOccurred && before.Scene)
            PushUndoSnapshot(std::move(before));
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
        uiState.SceneRequests.RequestUndo = false;
        uiState.SceneRequests.RequestRedo = false;
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

        uiState.SceneRequests.RequestResetMaterialParameter = false;
        uiState.SceneRequests.ResetMaterialParameter = {};
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
        // ProcessRequests 在帧末根据这个标记提交一次历史快照。
        mHistoryMutationOccurred = true;
        MarkSceneDirty();

        // Keep editing responsive: inspector drags/text input may generate many
        // mutations per second. Persist only through explicit save requests.

        if (mAfterMutationCallback)
            mAfterMutationCallback();
    }

    void XJEditorSceneController::ClearSceneReferences()
    {
        ClearHistory();
        mScene = nullptr;
        mSceneAsset.reset();
        mInstantiateContext = {};
        mSceneDirty = false;
        mHistoryMutationOccurred = false;
    }

    void XJEditorSceneController::ClearRuntimeReferences()
    {
        ClearSceneReferences();

        mDefaultTexture.reset();
        mDefaultSampler.reset();
    
        mBeforeDeleteCallback = {};
        mAfterMutationCallback = {};
        mBeforeOpenSceneCallback = {};
        mAfterOpenSceneCallback = {};
        mCanDeleteEntityCallback = {};
        mShouldExposeEntityCallback = {};
    }
}
