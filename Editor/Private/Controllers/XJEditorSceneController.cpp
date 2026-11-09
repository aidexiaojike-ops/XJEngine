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
#include <cmath>


namespace XJ
{

    namespace
    {
        void InvalidateGizmoHistoryInput(XJEditorUIState& state)//无效化工具输入
        {
            state.GizmoPointer = {};
            state.GizmoRendered = {};
            state.GizmoInteraction = {};
            state.TransformGizmo = {};
        
            ++state.GizmoEpoch;
        }

        bool SameGizmoRotation(const glm::dquat& a, const glm::dquat& b)//判断两个四元数是否"表示同一个旋转"的函数
        {
            //长度校验
            const double aLength = glm::length(a);
            const double bLength = glm::length(b);

            if (!std::isfinite(aLength) ||
                !std::isfinite(bLength) ||
                aLength < 1.0e-12 ||
                bLength < 1.0e-12)
            {
                return false;
            }
            //归一化后取点积的绝对值
            const double dot =
                std::abs(
                    glm::dot(
                        a / aLength,
                        b / bLength));

            // q 和 -q 代表同一个旋转。
            return dot >= 1.0 - 1.0e-12;
        }

        bool SameGizmoLocalPose(const XJGizmoTransformSnapshot& a,const XJGizmoTransformSnapshot& b)//判断两个轴同一个位置
        {
            return
                a.LocalPosition == b.LocalPosition &&
                a.LocalScale == b.LocalScale &&
                SameGizmoRotation(
                    glm::dquat(a.LocalRotation),
                    glm::dquat(b.LocalRotation));
        }

        bool SameGizmoContext(const XJGizmoTransformSnapshot& a, const XJGizmoTransformSnapshot& b)//两个轴的上下？
        {
            return
                a.EntityId == b.EntityId &&
                a.StructureRevision == b.StructureRevision &&
                a.ParentId == b.ParentId &&
                a.ParentMatrix == b.ParentMatrix &&
                SameGizmoRotation(
                    a.ParentRotation,
                    b.ParentRotation);
        }
    }


    bool XJEditorSceneController::BeginTransformPreview(XJEditorUIState& state, const XJGizmoRenderedFrame& source, XJTransformGizmoHandle handle, XJGizmoTransformSnapshot& snapshot)
    {
        const auto operation = GetGizmoOperation(handle);

        if (!mScene ||
            mTransformPreviewActive ||
            operation == XJTransformGizmoOperation::None ||
            !source.Valid ||
            source.Epoch != state.GizmoEpoch ||
            source.EntityId != state.Selection.SelectedEntity ||
            source.Space != state.TransformTools.Space ||
            source.SettingsRevision !=
                state.TransformTools.SettingsRevision ||
            state.WorkspaceReadOnly ||
            state.RuntimeInspection ||
            (mShouldExposeEntityCallback &&
             !mShouldExposeEntityCallback(source.EntityId)))
        {
            return false;
        }
        //从场景读取原始 transform
        XJGizmoTransformSnapshot original;

        if (!XJEditorSceneService::ReadGizmoTransform(*mScene, source.EntityId, original))
            return false;

        //渲染帧 vs 场景真实数据
        const double positionDifference = glm::length(original.WorldPosition - source.WorldPosition);

        if (!std::isfinite(positionDifference) ||  positionDifference > 1.0e-4)
            return false;

        // 旋转一致性校验（区分 Local/World）
        const glm::dquat expectedOrientation =
            source.Space == XJEditorTransformSpace::Local
                ? original.WorldRotation
                : glm::dquat(1.0, 0.0, 0.0, 0.0);

        if (!SameGizmoRotation(expectedOrientation, source.Orientation))
            return false;

         //捕获历史快照（撤销用）
        auto before = CaptureHistorySnapshot(state);

        if (!before.Scene)
            return false;
        //记录预览元信息 + 置位激活标志
        mTransformOriginal = original;
        mTransformBefore = std::move(before);

        mTransformPreviewOperation = operation;
        mTransformPreviewSpace = source.Space;

        mTransformPreviewActive = true;
        //输出原始快照并返回成功
        snapshot = original;
        return true;
    }

    bool XJEditorSceneController::PreviewTransform(XJEditorUIState& state, const XJGizmoPreviewValue& value)
    {
        if (!mScene ||
            !mTransformPreviewActive ||
            state.WorkspaceReadOnly ||
            state.RuntimeInspection ||
            state.Selection.SelectedEntity !=
                mTransformOriginal.EntityId)
        {
            return false;
        }
        //计算"期望空间"
        const auto expectedSpace =
            mTransformPreviewOperation ==
                XJTransformGizmoOperation::Move
                ? XJEditorTransformSpace::World
                : mTransformPreviewSpace;

        if (value.Operation != mTransformPreviewOperation || value.Space != expectedSpace)
        {
            state.GizmoInteraction.Error =
                "The Gizmo operation changed during a drag.";

            return false;
        }
        //一致性校验
        XJGizmoTransformSnapshot current;

        if (!XJEditorSceneService::ReadGizmoTransform(
                *mScene,
                mTransformOriginal.EntityId,
                current) ||
            !SameGizmoContext(mTransformOriginal, current))
        {
            state.GizmoInteraction.Error =
                "The scene hierarchy changed during a Gizmo drag.";

            return false;
        }
        //应用预览
        const auto result = XJEditorSceneService::ApplyGizmoPreview(*mScene, mTransformOriginal.EntityId, value);

        if (result.Rejected())
        {
            state.GizmoInteraction.Error = result.Error;
            return false;
        }

        if (result.Changed())
            InvalidateViewModels();

        return true;
    }

    bool XJEditorSceneController::EndTransformPreview(XJEditorUIState& state, bool commit)
    {
        if (!mTransformPreviewActive)
            return false;

        if (!commit)
            return CancelTransformPreview();

        XJGizmoTransformSnapshot current;

        if (!mScene ||
            !XJEditorSceneService::ReadGizmoTransform(
                *mScene,
                mTransformOriginal.EntityId,
                current) ||
            !SameGizmoContext(mTransformOriginal, current))
        {
            CancelTransformPreview();
            return false;
        }

        if (SameGizmoLocalPose(mTransformOriginal, current))
        {
            // 包含四元数末位误差的情况。
            // 恢复初始快照，避免留下未标脏的微小漂移。
            return CancelTransformPreview();
        }

        //移出快照 + 清状态
        auto before = std::move(mTransformBefore);
        
        mTransformPreviewActive = false;
        mTransformPreviewOperation = XJTransformGizmoOperation::None;

        mTransformOriginal = {};
        mTransformBefore = {};

        PushUndoSnapshot(std::move(before));
        NotifyAfterMutation();

        return true;
    }

    bool XJEditorSceneController::CancelTransformPreview()
    {
        if (!mTransformPreviewActive)
            return true;
    
        const auto original = mTransformOriginal;
    
        mTransformPreviewActive = false;
        mTransformPreviewOperation =
            XJTransformGizmoOperation::None;
    
        mTransformOriginal = {};
        mTransformBefore = {};
    
        const bool restored =
            mScene &&
            XJEditorSceneService::RestoreGizmoTransform(
                *mScene,
                original);
            
        InvalidateViewModels();
            
        if (!restored)
        {
            // 正常入口会在销毁实体/场景前取消。
            // 到这里说明有其他路径提前破坏了目标。
            spdlog::warn(
                "Could not restore Gizmo preview "
                "for entity={}.",
                original.EntityId);
            
            // 不能把可能残留的预览状态假装成干净场景。
            if (mScene)
                MarkSceneDirty();
        }
    
        return restored;
    }

    void XJEditorSceneController::SetScene(XJScene* scene)
    {
        if (mScene != scene)
        {
            ++mSceneRevision;
            CancelTransformPreview();
        }
        
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
    
    bool XJEditorSceneController::LoadOrCreateDefaultScene(
        XJEditorUIState& uiState,
        XJAssetHandle defaultSceneHandle,
        XJAssetHandle defaultMeshHandle,
        XJAssetHandle defaultComponentMeshHandle,
        const std::filesystem::path& scenePath,
        const std::filesystem::path& resourceRoot,
        const std::filesystem::path& registryPath)
    {
        if (!mScene || !mAssetRegistry)
            return false;

        XJAssetBootstrap bootstrap(
            *mAssetRegistry,
            defaultSceneHandle,
            defaultMeshHandle,
            defaultComponentMeshHandle,
            resourceRoot,
            registryPath,
            scenePath);
        if (!bootstrap.LoadOrCreateAssetRegistry())
        {
            spdlog::error("Default scene bootstrap failed.");
            return false;
        }
        
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
        CancelTransformPreview();

        if (!mScene || !mAssetRegistry)
            return false;
        //读取场景
        auto sceneAsset = XJSceneAssetSerializer::LoadFromFile(scenePath);
        if (!sceneAsset)
        {
            spdlog::error("Failed to load scene: {}", scenePath.string());
            return false;
        }

        // 正式资产记录保存 Handle/Name/Path；运行时快照只用于失败回滚。
        std::shared_ptr<XJSceneAsset> previousSceneAssetRecord = mSceneAsset;
        std::shared_ptr<XJSceneAsset> previousRuntimeSnapshot =
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

        bool targetSceneModified = false;
        if (!InstantiateSceneAsset(sceneAsset, sceneHandle, &targetSceneModified))
        {
            spdlog::error("Failed to instantiate scene: {}", scenePath.string());

            if (targetSceneModified && previousRuntimeSnapshot)
            {
                // Roll back to the scene that was visible before the failed open.
                if (!InstantiateSceneAsset(previousRuntimeSnapshot, previousSceneHandle))
                {
                    spdlog::error("Failed to roll back previous scene after open failure.");
                }
            }

            mSceneAsset = previousSceneAssetRecord;
            mCurrentScenePath = previousScenePath;
            mSceneDirty = previousSceneDirty;

            if (mAfterOpenSceneCallback && mScene)
                mAfterOpenSceneCallback(*mScene);

            ++mSceneRevision;
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
        ++mSceneRevision;
        RefreshViewModels(uiState);
        return true;  
    }

    void XJEditorSceneController::MarkSceneDirty()
    {
        mSceneDirty = true;
    }

    bool XJEditorSceneController::SaveCurrentScene()
    {
        if (mTransformPreviewActive)
            return false;

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

        const bool sceneChanged = mPublishedRevision != mSceneRevision;
        const bool assetDetailsChanged =
            mPublishedAssetDetailEpoch != uiState.AssetDetailEpoch;

        const bool selectionChanged = mPublishedSelectedEntity != uiState.Selection.SelectedEntity || mPublishedSelectedAsset != uiState.Selection.SelectedAsset;

        if (!sceneChanged && !selectionChanged && !assetDetailsChanged)
        {
            return;
        }
        if (!mScene)
        {
            uiState.SceneView = {};
            uiState.SelectedEntityDetails = {};
            mPublishedRevision = mSceneRevision;
            mPublishedAssetDetailEpoch = uiState.AssetDetailEpoch;
            mPublishedSelectedEntity = uiState.Selection.SelectedEntity;
            mPublishedSelectedAsset = uiState.Selection.SelectedAsset;
            return;
        }

        if (mTransformPreviewActive)
        {
            ResetSceneRequestState(uiState);
            return;
        }

    
        if (sceneChanged)
        {
            uiState.SceneView = XJEditorSceneService::BuildSceneViewModel(
                *mScene,
                mShouldExposeEntityCallback);
        }
        
        if (uiState.Selection.SelectedEntity != XJ_INVALID_EDITOR_ENTITY_ID)
        {
            if (mShouldExposeEntityCallback &&
                !mShouldExposeEntityCallback(uiState.Selection.SelectedEntity))
            {
                uiState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
                uiState.SelectedEntityDetails = {};
            }
            else
            {
                uiState.SelectedEntityDetails = XJEditorSceneService::BuildEntityDetailsView(
                *mScene,
                uiState.Selection.SelectedEntity,
                mAssetRegistry,
                mShouldExposeEntityCallback);
            }
        }
        else
        {
            uiState.SelectedEntityDetails = {};
        }

        mPublishedRevision = mSceneRevision;
        mPublishedAssetDetailEpoch = uiState.AssetDetailEpoch;
        mPublishedSelectedEntity = uiState.Selection.SelectedEntity;
        mPublishedSelectedAsset = uiState.Selection.SelectedAsset;
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
        // / 重建场景前解除视口引用
        const XJAssetHandle sceneHandle = mInstantiateContext.SourceScene.Handle;

        if (mBeforeOpenSceneCallback)
            mBeforeOpenSceneCallback();

        if (!InstantiateSceneAsset(snapshot.Scene, sceneHandle))
        {
            // 现有实例化失败路径也要重新验证和绑定当前场景引用。
            if (mAfterOpenSceneCallback)
                mAfterOpenSceneCallback(*mScene);
        
            uiState.SelectedEntityDetails = {};
            InvalidateViewModels();
        
            return false;
        }

        uiState.Selection = snapshot.Selection;
        uiState.SelectedEntityDetails = {};

        // 预览相机不会写入场景快照，恢复后通过现有回调重新创建并绑定视口。
        if (mAfterOpenSceneCallback)
            mAfterOpenSceneCallback(*mScene);

        MarkSceneDirty();
        ++mSceneRevision;
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
        CancelTransformPreview();
        InvalidateGizmoHistoryInput(uiState);

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
        CancelTransformPreview();

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
        CancelTransformPreview();
        mUndoStack.clear();
        mRedoStack.clear();
    }

    bool XJEditorSceneController::ExecuteExternalMutation(
        XJEditorUIState& uiState,
        const std::function<bool()>& action)
    {
        if (mTransformPreviewActive)
            return false;

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
            uiState.SceneRequests.RequestUpdateLight ||
            uiState.SceneRequests.RequestAddScriptSlot ||
            uiState.SceneRequests.RequestRemoveScriptSlot ||
            uiState.SceneRequests.RequestSetScriptSlotEnabled ||
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
                added = XJEditorSceneService::AddComponent(*mScene, request.EntityId, request.ComponentType, mInstantiateContext.SourceScene);
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

        if (uiState.SceneRequests.RequestAddScriptSlot)
        {
            const auto request = uiState.SceneRequests.AddScriptSlot;
            uiState.SceneRequests.RequestAddScriptSlot = false;
            uiState.SceneRequests.AddScriptSlot = {};
            if (mAssetRegistry && XJEditorSceneService::AddScriptSlot(
                    *mScene, request.EntityId, request.ScriptAsset, *mAssetRegistry))
            {
                NotifyAfterMutation();
            }
        }

        if (uiState.SceneRequests.RequestRemoveScriptSlot)
        {
            const auto request = uiState.SceneRequests.RemoveScriptSlot;
            uiState.SceneRequests.RequestRemoveScriptSlot = false;
            uiState.SceneRequests.RemoveScriptSlot = {};
            if (XJEditorSceneService::RemoveScriptSlot(
                    *mScene, request.EntityId, request.SlotId))
            {
                NotifyAfterMutation();
            }
        }

        if (uiState.SceneRequests.RequestSetScriptSlotEnabled)
        {
            const auto request = uiState.SceneRequests.SetScriptSlotEnabled;
            uiState.SceneRequests.RequestSetScriptSlotEnabled = false;
            uiState.SceneRequests.SetScriptSlotEnabled = {};
            if (XJEditorSceneService::SetScriptSlotEnabled(
                    *mScene, request.EntityId, request.SlotId, request.Enabled))
            {
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
            //更新数据
            const auto request = uiState.SceneRequests.UpdateTransform;
            uiState.SceneRequests.RequestUpdateTransform = false;
            uiState.SceneRequests.UpdateTransform = {};
            uiState.TransformEditFeedback = {};

            XJEditorTransformEditResult result;

            const bool allowed =
                !uiState.WorkspaceReadOnly &&
                !uiState.RuntimeInspection &&
                (!mShouldExposeEntityCallback ||
                    mShouldExposeEntityCallback(
                        request.EntityId));

            if (!allowed)
            {
                result = {
                    XJEditorTransformEditOutcome::Rejected,
                    "Transform editing is unavailable "
                    "for this entity or workspace mode."
                };
            }
            else
            {
                result =
                    XJEditorSceneService::UpdateTransform(
                        *mScene,
                        request);
            }

            if (result.Rejected())
            {
                uiState.TransformEditFeedback.EntityId = request.EntityId;
                uiState.TransformEditFeedback.Space = request.Space;
                uiState.TransformEditFeedback.Error = result.Error;
                spdlog::warn(
                    "Transform edit rejected for entity={}: {}",
                    request.EntityId,
                    result.Error);
            }
            else if (result.Changed())
            {
                NotifyAfterMutation();
            }
        }

        if (uiState.SceneRequests.RequestUpdateCamera)
        {
            auto request = uiState.SceneRequests.UpdateCamera;
            uiState.SceneRequests.RequestUpdateCamera = false;
            uiState.SceneRequests.UpdateCamera = {};

            XJ::XJEditorSceneService::UpdateCamera(*mScene, request);

           NotifyAfterMutation();
        }

        if (uiState.SceneRequests.RequestUpdateLight)
        {
            auto request = uiState.SceneRequests.UpdateLight;
            uiState.SceneRequests.RequestUpdateLight = false;
            uiState.SceneRequests.UpdateLight = {};

            XJ::XJEditorSceneService::UpdateLight(*mScene, request);

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
        //灯光
        uiState.SceneRequests.RequestUpdateLight = false;
        uiState.SceneRequests.UpdateLight = {};

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

        uiState.SceneRequests.RequestAddScriptSlot = false;
        uiState.SceneRequests.AddScriptSlot = {};
        uiState.SceneRequests.RequestRemoveScriptSlot = false;
        uiState.SceneRequests.RemoveScriptSlot = {};
        uiState.SceneRequests.RequestSetScriptSlotEnabled = false;
        uiState.SceneRequests.SetScriptSlotEnabled = {};
    }

    void XJEditorSceneController::ResetSelectionForScene(XJEditorUIState& uiState, XJAssetHandle sceneHandle)
    {
        uiState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
        uiState.Selection.SelectedAsset = sceneHandle;
        uiState.Selection.HighlightedEntities.clear();//清楚高亮

        uiState.SelectedEntityDetails = {};//删除选择数据
    }
    
    bool XJEditorSceneController::InstantiateSceneAsset(
        std::shared_ptr<XJSceneAsset> sceneAsset,
        XJAssetHandle sceneHandle,
        bool* outSceneModified)
    {
        if (outSceneModified)
            *outSceneModified = false;

        if(!mScene || !sceneAsset || !mAssetRegistry)
            return false;

        XJSceneInstantiateContext nextContext;
        nextContext.Registry = mAssetRegistry;
        nextContext.SourceScene = { sceneHandle, XJAssetType::Scene };
        nextContext.DefaultTexture = mDefaultTexture;
        nextContext.DefaultSampler = mDefaultSampler;

        // EntityMap 仅供实例化过程使用。开始潜在的 Scene 替换前，先清除旧裸指针。
        mInstantiateContext.EntityMap.clear();

        if (!XJSceneInstantiator::Instantiate(*sceneAsset, *mScene, &nextContext))
        {
            if (outSceneModified)
                *outSceneModified = nextContext.TargetSceneModified;
            return false;
        }

        mInstantiateContext = std::move(nextContext);
        if (outSceneModified)
            *outSceneModified = true;
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
        ++mSceneRevision;
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
