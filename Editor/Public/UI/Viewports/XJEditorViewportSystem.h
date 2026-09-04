#ifndef XJ_EDITOR_VIEWPORT_SYSTEM_H
#define XJ_EDITOR_VIEWPORT_SYSTEM_H

#include "Graphic/VulkanCommon.h"
#include "UI/XJEditorSelection.h"
#include "UI/XJEditorPlayMode.h"
#include <vector>

#include <memory>

namespace XJ
{
    class XJGlfwWindow;
    class XJRenderContext;
    class XJRenderTarget;
    class XJScene;
    class XJEntity;
    class XJScenePreview;
    class XJGamePreview;
    class XJEditorCameraController;
    class XJEditorCameraManager;
    class XJEditorRenderResources;

    struct XJEditorViewportSystemInitInfo
    {
        XJGlfwWindow* Window = nullptr;
        XJRenderContext* RenderContext = nullptr;
        XJRenderTarget* MainRenderTarget = nullptr;
        XJEditorRenderResources* Resources = nullptr;
    };

    class XJEditorViewportSystem
    {
        public:
            XJEditorViewportSystem();
            ~XJEditorViewportSystem();

            XJEditorViewportSystem(const XJEditorViewportSystem&) = delete;
            XJEditorViewportSystem& operator=(const XJEditorViewportSystem&) = delete;

            bool Init(const XJEditorViewportSystemInitInfo& info);

            bool AttachScene(XJScene& scene);
            void DetachScene(XJScene* scene);

            void DrawUI();
            void Update(float deltaTime);
            void OnMouseScroll(float yOffset);

            void Shutdown();

            XJScenePreview* GetScenePreview() const;
            XJGamePreview* GetGamePreview() const;
            //相机接口
            void BeforeDeleteEntities(XJScene& scene, const std::vector<XJEditorEntityId>& ids);

            void BeforeOpenScene();
            void AfterOpenScene(XJScene& scene);
            void RefreshSceneCameras();

            bool IsProtectedEditorEntity(XJEditorEntityId id) const;

            void SetPlayState(XJEditorPlayState state);
            void BeginPlay(XJScene* runtimeScene, XJEntity* runtimeCamera);// Play 时 GamePreview 切到运行时克隆
            void EndPlay();                                                  // Stop 时恢复编辑器场景与相机

        private:
            class Impl;
            std::unique_ptr<Impl> mImpl;
    };
}

#endif