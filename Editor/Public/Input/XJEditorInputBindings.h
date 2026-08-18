#ifndef XJ_EDITOR_INPUT_BINDINGS_H
#define XJ_EDITOR_INPUT_BINDINGS_H

#include <memory>

namespace XJ
{
    class XJGlfwWindow;
    class XJEditorWorkspace;
    class XJEditorViewportSystem;

    struct XJEditorInputBindingsInitInfo
    {
        XJGlfwWindow* Window = nullptr;
        XJEditorWorkspace* Workspace = nullptr;
        XJEditorViewportSystem* Viewports = nullptr;
    };

    class XJEditorInputBindings
    {
        public:
            XJEditorInputBindings();
            ~XJEditorInputBindings();
        
            XJEditorInputBindings(
                const XJEditorInputBindings&) = delete;
            XJEditorInputBindings& operator=(
                const XJEditorInputBindings&) = delete;
            
            bool Init(
                const XJEditorInputBindingsInitInfo& info);
            
            // 所有 UI drop target 绘制后调用，清理无人消费的外部拖放。
            void EndUIFrame();
            
            void Shutdown();
            
        private:
            class Impl;
            std::unique_ptr<Impl> mImpl;
    };
}

#endif