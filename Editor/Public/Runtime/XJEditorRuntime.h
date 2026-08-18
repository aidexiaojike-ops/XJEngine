#ifndef XJ_EDITOR_RUNTIME_H
#define XJ_EDITOR_RUNTIME_H

#include "Runtime/XJEditorProjectConfig.h"
#include <memory>

namespace XJ
{
    class XJGlfwWindow;
    class XJRenderContext;
    class XJScene;
    class XJEditorFrameRenderer;
    class XJEditorWorkspace;
    class XJEditorViewportSystem;
    class XJEditorUIHost;
    class XJEditorRenderResources;
    class XJEditorInputBindings;
    class XJEditorFrameRenderer;

    struct XJEditorRuntimeInitInfo
    {
        XJGlfwWindow* Window = nullptr;
        XJRenderContext* RenderContext = nullptr;
        XJEditorProjectConfig Config;
    };

    class XJEditorRuntime
    {
        public:
            XJEditorRuntime();
            ~XJEditorRuntime();

            XJEditorRuntime(const XJEditorRuntime&) = delete;
            XJEditorRuntime& operator=(const XJEditorRuntime&) = delete;

            bool Init(const XJEditorRuntimeInitInfo& info);
            bool AttachScene(XJScene& scene);
            void DetachScene(XJScene* scene);

            void BeginUI();
            void Update(float deltaTime);
            void EndUI();
            void Render();

            void ShutdownUI();
            void Shutdown();

        private:
            class Impl;
            std::unique_ptr<Impl> mImpl;
    };
}

#endif