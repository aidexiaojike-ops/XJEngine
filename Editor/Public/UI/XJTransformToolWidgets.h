#ifndef XJ_TRANSFORM_TOOL_WIDGETS_H
#define XJ_TRANSFORM_TOOL_WIDGETS_H

namespace XJ
{
    struct XJEditorUIState;

    class XJTransformToolWidgets
    {
        public:
            // 只读取 ViewModel、提交请求。
            // 不直接更新工具设置，不修改 ECS。
            static void DrawSpaceButton(XJEditorUIState& state);
    };
}

#endif