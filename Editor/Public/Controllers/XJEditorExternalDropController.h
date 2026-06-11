//处理操作系统文件拖入窗口
#ifndef XJ_EDITOR_EXTERNAL_DROP_CONTROLLER_H
#define XJ_EDITOR_EXTERNAL_DROP_CONTROLLER_H

struct GLFWwindow;

namespace XJ
{
    struct XJEditorUIState;

    class XJEditorExternalDropController
    {
        public:
            void OnExternalFilesDropped(XJEditorUIState& uiState, GLFWwindow* window, int count, const char** paths);
    };
}

#endif