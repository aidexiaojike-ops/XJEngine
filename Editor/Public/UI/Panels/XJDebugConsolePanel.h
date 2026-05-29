//Debug 输出  显示日志、警告、错误，支持过滤和清空，类似 Output Log
#ifndef XJ_DEBUG_CONSOLE_PANEL_H
#define XJ_DEBUG_CONSOLE_PANEL_H

#include "UI/XJEditorUIConfig.h"

namespace XJ
{
    class XJEditorUIState;

    class XJDebugConsolePanel
    {
        public:
            XJDebugConsolePanel(XJEditorUIState& state, XJEditorPanelConfig_DebugConsole* config);
            ~XJDebugConsolePanel();

            void DrawUI();

        private:
            XJEditorUIState& mState;
            XJEditorPanelConfig_DebugConsole* mConfig = nullptr;
            bool mShowTrace = true;
            bool mShowCritical = true;
            char mSearchBuffer[256] = {};
    };
}

#endif
