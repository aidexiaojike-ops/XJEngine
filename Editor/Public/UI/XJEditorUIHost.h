#ifndef XJ_EDITOR_UI_HOST_H
#define XJ_EDITOR_UI_HOST_H

#include <memory>
#include <filesystem>

struct ImDrawData;

namespace XJ
{
    class XJGlfwWindow;
    class XJRenderContext;
    class XJEditorFrameRenderer;
    class XJUIContext;
    class XJEditorRenderer;
    class XJEditorUILayer;
    struct XJEditorUIState;

    struct XJEditorUIHostInitInfo
    {
        XJGlfwWindow* Window = nullptr;
        XJRenderContext* RenderContext = nullptr;
        XJEditorFrameRenderer* FrameRenderer = nullptr;
        XJEditorUIState* UIState = nullptr;

        std::filesystem::path ConfigPath;
        std::filesystem::path ProjectResourceRoot;
        std::filesystem::path ImGuiIniPath;
    };

    class XJEditorUIHost
    {
        public:
            XJEditorUIHost();
            ~XJEditorUIHost();

            XJEditorUIHost(const XJEditorUIHost&) = delete;
            XJEditorUIHost& operator=(const XJEditorUIHost&) = delete;

            bool Init(const XJEditorUIHostInitInfo& info);
            void BeginFrame();
            void EndFrame();
            void Shutdown();
            void DrawUI();

            ImDrawData* GetDrawData() const;
            XJEditorRenderer* GetRenderer() const;
            bool IsInitialized() const;

        private:
            XJRenderContext* mRenderContext = nullptr;

            // Renderer 必须先于 ImGui context 关闭。
            std::unique_ptr<XJUIContext> mContext;
            std::unique_ptr<XJEditorRenderer> mRenderer;
            // UILayer 持有 UIState 引用，所以 Workspace 必须晚于 UIHost 销毁。
            std::unique_ptr<XJEditorUILayer> mLayer;

            bool mInitialized = false;
    };
}

#endif
