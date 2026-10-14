#ifndef XJ_EDITOR_VIEWPORT_SYSTEM_H
#define XJ_EDITOR_VIEWPORT_SYSTEM_H

#include "Graphic/VulkanCommon.h"
#include "UI/XJEditorSelection.h"
#include "Runtime/XJEditorPlayState.h"
#include "Diagnostics/XJPerformanceData.h"
#include "Render/XJRenderViewOptions.h"
#include "Viewport/XJEditorViewportTypes.h"

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
    struct XJEditorUIState;

    struct XJEditorViewportSystemInitInfo
    {
        XJGlfwWindow* Window = nullptr;
        XJRenderContext* RenderContext = nullptr;
        XJRenderTarget* MainRenderTarget = nullptr;
        XJEditorRenderResources* Resources = nullptr;
        XJEditorUIState* UIState = nullptr;
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
            void SetPreviewOrbitPivotFromRay(
                const glm::vec3& rayOrigin,
                const glm::vec3& rayDirection,
                float maxDistance);

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

            std::weak_ptr<void> GetLifetimeToken() const;
            void BeginStatisticsFrame(uint64_t frameId);//开始统计帧

            XJRenderStatistics ReadStatistics(XJEditorViewportId viewport) const;//读取统计信息
            XJRenderViewCapabilities ReadViewCapabilities(XJEditorViewportId viewport) const;//读取视图能力
            bool IsRenderViewAvailable(XJEditorViewportId viewport) const;//检查渲染视图是否可用
            bool ApplyViewOptions(XJEditorViewportId viewport, const XJRenderViewOptions& options);//应用视图选项
        private:
            class Impl;
            std::unique_ptr<Impl> mImpl;
            XJRenderTarget* ResolveRenderTarget(XJEditorViewportId viewport) const;
    };
}

#endif
