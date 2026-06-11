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

        XJEntity* previewCamera = scene->CreateEntityWithUUID(previewCameraUuid, "PreviewCamera");

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
        if (!scene)
        {
            ClearAllCameraReferences();
            return;
        }

        mGameCameraEntity = XJSceneRuntimeUtil::FindPrimaryCameraEntity(*scene);
        mPreviewCameraEntity = EnsurePreviewCamera(scene, previewCameraEntityId);

        ApplyCameraBindings();
    }
     void XJEditorCameraManager::ClearAllCameraReferences()
    {
        mPreviewCameraEntity = nullptr;
        mGameCameraEntity = nullptr;

        ApplyCameraBindings();
    }

    void XJEditorCameraManager::ClearIfDeleted(
        XJScene& scene,
        const std::vector<XJEditorEntityId>& ids)
    {
        for (XJEditorEntityId id : ids)
        {
            XJEntity* entity = XJEditorSceneService::FindEntityById(scene, id);
            if (!entity)
                continue;

            if (mPreviewCameraEntity == entity)
                mPreviewCameraEntity = nullptr;

            if (mGameCameraEntity == entity)
                mGameCameraEntity = nullptr;
        }

        ApplyCameraBindings();
    }

    void XJEditorCameraManager::ValidateCameraPointers()
    {
        if (mPreviewCameraEntity && !mPreviewCameraEntity->IsValid())
            mPreviewCameraEntity = nullptr;

        if (mGameCameraEntity && !mGameCameraEntity->IsValid())
            mGameCameraEntity = nullptr;

        ApplyCameraBindings();
    }

    void XJEditorCameraManager::OnMouseScroll(float yOffset)
    {
        if (!mScenePreview || !mScenePreview->IsHovered())
            return;

        if (!mCameraController)
            return;

        if (!mPreviewCameraEntity ||
            !XJEntity::HasComponent<XJCameraComponent>(mPreviewCameraEntity))
            return;

        mCameraController->OnMouseScroll(yOffset, mPreviewCameraEntity);
    }

    void XJEditorCameraManager::UpdatePreviewCameraControl(
        float deltaTime,
        XJGlfwWindow* window)
    {
        if (!mScenePreview || !mScenePreview->ShouldControlCamera())
            return;

        if (!mCameraController || !window)
            return;

        if (!mPreviewCameraEntity ||
            !XJEntity::HasComponent<XJCameraComponent>(mPreviewCameraEntity))
            return;

        mCameraController->UpdateCameraControl(deltaTime, window, mPreviewCameraEntity);
    }

    XJEntity* XJEditorCameraManager::GetPreviewCamera() const
    {
        return mPreviewCameraEntity;
    }

    XJEntity* XJEditorCameraManager::GetGameCamera() const
    {
        return mGameCameraEntity;
    }
    
    bool XJEditorCameraManager::IsPreviewCamera(XJEditorEntityId id) const
    {
        return mPreviewCameraEntity &&
               static_cast<XJEditorEntityId>(mPreviewCameraEntity->XJGetUUID()) == id;
    }

    bool XJEditorCameraManager::IsProtectedEditorCamera(XJEditorEntityId id) const
    {
        return IsPreviewCamera(id);
    }

    void XJEditorCameraManager::ApplyCameraBindings()
    {
        if (mScenePreview)
            mScenePreview->SetCamera(mPreviewCameraEntity);

        if (mGamePreview)
            mGamePreview->SetCamera(mGameCameraEntity);

        if (mRenderTarget)
            mRenderTarget->XJSetCamera(mGameCameraEntity);
    }
}