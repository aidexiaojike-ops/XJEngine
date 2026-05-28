//Debug 输出  显示日志、警告、错误，支持过滤和清空，类似 Output Log
#ifndef XJ_DEBUG_CONSOLE_PANEL_H
#define XJ_DEBUG_CONSOLE_PANEL_H


namespace XJ
{
    class XJEditorUIState;

    class XJDebugConsolePanel
    {
        public:
            XJDebugConsolePanel(XJEditorUIState& state);
            ~XJDebugConsolePanel();

            void SetState(XJEditorUIState* state) const { if (state) mState = *state;};
            void DrawUI();

        private:
            XJEditorUIState& mState;
    };
}

#endif
