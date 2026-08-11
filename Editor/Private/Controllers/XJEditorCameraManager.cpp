#include "Controllers/XJEditorCameraManager.h"

#include "Asset/XJSceneRuntimeUtil.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"
#include "ECS/XJUUID.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "Controllers/XJEditorCameraController.h"

#include "Render/XJRenderTarget.h"

#include "Services/XJEditorSceneService.h"
#include "UI/Viewports/XJGamePreview.h"
#include "UI/Viewports/XJScenePreview.h"


namespace XJ
{
    void XJEditorCameraManager::BindViewports(XJScenePreview* scenePreview, XJGamePreview* gamePreview, XJRenderTarget* renderTarget)
    {
        mScenePreview = scenePreview;
        mGamePreview = gamePreview;
        mRenderTarget = renderTarget;

        ApplyCameraBindings();
    }

    void XJEditorCameraManager::BindCameraController(XJEditorCameraController* cameraController)//绑定摄像机控制器
    {
        mCameraController = cameraController;
    }

    XJEntity* XJEditorCameraManager::EnsurePreviewCamera(XJScene* scene,uint64_t previewCameraEntityId)//默认编辑器相机设置
    {
        if (!scene)
            return nullptr;

        const XJUUID previewCameraUuid(static_cast<uint64_t>(previewCameraEntityId));

        for (const auto& [enttEntity, entity] : scene->GetEntities())
        {
            if (entity && entity->XJGetUUID() == previewCameraUuid)
                return entity.get();
        }

        XJEntity* previewCamera = scene->CreateEntityWithUUIDAndTransform(previewCameraUuid, "PreviewCamera");

        if (!previewCamera)
            return nullptr;

        auto& transform = previewCamera->GetComponent<XJTransformComponent>();
        transform.position = glm::vec3(0.0f, 1.5f, 3.0f);
        transform.rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
        transform.scale = glm::vec3(1.0f);
        transform.UpdateModelMatrix();

        auto& camera = previewCamera->AddComponent<XJCameraComponent>();
        camera.XJSetFov(65.0f);
        camera.XJSetNear(0.1f);
        camera.XJSetFar(100.0f);

        return previewCamera;
    }

    void XJEditorCameraManager::SetupCamerasForScene(XJScene* scene, uint64_t previewCameraEntityId)
    {
        mScene = scene;

        if (!scene)
        {
            ClearAllCameraReferences();
            return;
        }

        XJEntity* gameCamera = XJSceneRuntimeUtil::FindPrimaryCameraEntity(*scene);
        XJEntity* previewCamera = EnsurePreviewCamera(scene, previewCameraEntityId);

        mGameCameraId = gameCamera ? static_cast<XJEditorEntityId>(gameCamera->XJGetUUID()) : XJ_INVALID_EDITOR_ENTITY_ID;
        mPreviewCameraId = previewCamera ? static_cast<XJEditorEntityId>(previewCamera->XJGetUUID()) : XJ_INVALID_EDITOR_ENTITY_ID;

        ApplyCameraBindings();
    }
     void XJEditorCameraManager::ClearAllCameraReferences()
    {
        mScene = nullptr;
        mPreviewCameraId = XJ_INVALID_EDITOR_ENTITY_ID;
        mGameCameraId = XJ_INVALID_EDITOR_ENTITY_ID;

        ApplyCameraBindings();
    }

    void XJEditorCameraManager::ClearIfDeleted(
        XJScene& scene,
        const std::vector<XJEditorEntityId>& ids)
    {
        for (XJEditorEntityId id : ids)
        {
            if (mPreviewCameraId == id)
                mPreviewCameraId = XJ_INVALID_EDITOR_ENTITY_ID;

            if (mGameCameraId == id)
                mGameCameraId = XJ_INVALID_EDITOR_ENTITY_ID;
        }

        ApplyCameraBindings();
    }

    void XJEditorCameraManager::ValidateCameraPointers()
    {
        if (mPreviewCameraId != XJ_INVALID_EDITOR_ENTITY_ID && !ResolveEntity(mPreviewCameraId))
            mPreviewCameraId = XJ_INVALID_EDITOR_ENTITY_ID;

        if (mGameCameraId != XJ_INVALID_EDITOR_ENTITY_ID && !ResolveEntity(mGameCameraId))
            mGameCameraId = XJ_INVALID_EDITOR_ENTITY_ID;

        ApplyCameraBindings();
    }

    void XJEditorCameraManager::OnMouseScroll(float yOffset)
    {
        if (!mScenePreview || !mScenePreview->IsHovered())
            return;

        if (!mCameraController)
            return;

        XJEntity* previewCamera = ResolveEntity(mPreviewCameraId);
        if (!previewCamera ||
            !XJEntity::HasComponent<XJCameraComponent>(previewCamera))
            return;

        mCameraController->OnMouseScroll(yOffset, previewCamera);
    }

    void XJEditorCameraManager::UpdatePreviewCameraControl(
        float deltaTime,
        XJGlfwWindow* window)
    {
        if (!mScenePreview || !mScenePreview->ShouldControlCamera())
            return;

        if (!mCameraController || !window)
            return;

        XJEntity* previewCamera = ResolveEntity(mPreviewCameraId);
        if (!previewCamera ||
            !XJEntity::HasComponent<XJCameraComponent>(previewCamera))
            return;

        mCameraController->UpdateCameraControl(deltaTime, window, previewCamera);
    }

    XJEntity* XJEditorCameraManager::GetPreviewCamera() const
    {
        return ResolveEntity(mPreviewCameraId);
    }

    XJEntity* XJEditorCameraManager::GetGameCamera() const
    {
        return ResolveEntity(mGameCameraId);
    }
    
    bool XJEditorCameraManager::IsPreviewCamera(XJEditorEntityId id) const
    {
        return mPreviewCameraId == id;
    }

    bool XJEditorCameraManager::IsProtectedEditorCamera(XJEditorEntityId id) const
    {
        return IsPreviewCamera(id);
    }

    void XJEditorCameraManager::ApplyCameraBindings()
    {
        XJEntity* previewCamera = ResolveEntity(mPreviewCameraId);
        XJEntity* gameCamera = ResolveEntity(mGameCameraId);

        if (mScenePreview)
            mScenePreview->SetCamera(previewCamera);

        if (mGamePreview)
            mGamePreview->SetCamera(gameCamera);

        if (mRenderTarget)
        {
            if (gameCamera)
                mRenderTarget->XJSetCamera(gameCamera);
            else
                mRenderTarget->XJClearCamera();
        }
    }

    XJEntity* XJEditorCameraManager::ResolveEntity(XJEditorEntityId id) const
    {
        if (!mScene || id == XJ_INVALID_EDITOR_ENTITY_ID)
            return nullptr;

        // Store camera references as UUIDs. Raw XJEntity* may be freed by
        // XJScene::DestroyEntity, so resolve a fresh pointer only at use sites.
        return XJEditorSceneService::FindEntityById(*mScene, id);
    }
}
