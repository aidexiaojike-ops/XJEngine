#ifndef XJ_VIEWPORT_DISPLAY_WIDGETS_H
#define XJ_VIEWPORT_DISPLAY_WIDGETS_H

#include "Viewport/XJEditorViewportTypes.h"
#include <imgui.h>
//UI 的菜单和浮层组件
namespace XJ
{
    struct XJEditorUIState;

    class XJViewportDisplayWidgets
    {
        public:
            static bool DrawMenu(
                XJEditorUIState& state,
                XJEditorViewportId viewport);

            static void DrawPerformance(
                const XJEditorUIState& state,
                XJEditorViewportId viewport,
                const ImVec2& imagePosition,
                const ImVec2& imageSize);
    };
}

#endif