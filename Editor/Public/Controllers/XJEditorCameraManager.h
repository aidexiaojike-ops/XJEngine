#ifndef XJ_EDITOR_CAMERA_MANAGER_H
#define XJ_EDITOR_CAMERA_MANAGER_H

#include "UI/XJEditorSelection.h"

#include <cstdint>
#include <vector>



namespace XJ
{
    class XJEditorCameraController;
    class XJEntity;
    class XJGamePreview;
    class XJGlfwWindow;
    class XJRenderTarget;
    class XJScene;
    class XJScenePreview;

    class XJEditorCameraManager
    {
        public:
            void BindViewports(XJScenePreview* scenePreview, XJGamePreview* gamePreview, XJRenderTarget* renderTarget); //绑定视口 
            void BindCameraController(XJEditorCameraController* cameraController);  //绑定相机控制
            XJEntity* EnsurePreviewCamera(XJScene* scene, uint64_t previewCameraEntityId);  //确保编辑器窗口
            void SetupCamerasForScene(XJScene* scene, uint64_t previewCameraEntityId);  //更新摄像机和场景
            void ClearAllCameraReferences();    //删除摄像机
            void ClearIfDeleted(XJScene& scene, const std::vector<XJEditorEntityId>& ids);  //清理摄像机 卸载
            void ValidateCameraPointers();  //验证摄像机
            void OnMouseScroll(float yOffset);  //控制
            void UpdatePreviewCameraControl(float deltaTime, XJGlfwWindow* window); //更新编辑器摄像机
            
            XJEntity* GetPreviewCamera() const;//获取编辑器摄像机
            XJEntity* GetGameCamera() const;    //获取游戏摄像机
            bool IsPreviewCamera(XJEditorEntityId id) const;
            bool IsProtectedEditorCamera(XJEditorEntityId id) const;

        private:
            void ApplyCameraBindings(); 
            //窗口
            XJScenePreview* mScenePreview = nullptr;
            XJGamePreview* mGamePreview = nullptr;
            XJRenderTarget* mRenderTarget = nullptr;
            XJEditorCameraController* mCameraController = nullptr;  
            //摄像机ecs
            XJEntity* mPreviewCameraEntity = nullptr;
            XJEntity* mGameCameraEntity = nullptr;
    };


}

#endif