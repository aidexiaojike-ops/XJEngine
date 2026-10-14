#ifndef XJ_EDITOR_VIEWPORT_REQUESTS_H
#define XJ_EDITOR_VIEWPORT_REQUESTS_H

#include "Viewport/XJEditorViewportTypes.h"
#include <variant>

namespace XJ
{
    struct XJSetPerformanceVisible//设置性能可见
    {
        bool Visible = false;
    };

    struct XJSetFrameGraphVisible//设置帧图可见
    {
        bool Visible = false;
    };

    struct XJSetViewportRenderMode//设置视口渲染模式
    {
        XJEditorRenderMode Mode = XJEditorRenderMode::Final;
    };

    struct XJSetViewportDepthRange//设置视口深度范围
    {
        float Range = 10.0f;
    };

    struct XJResetViewportPerformance {};//重置视口性能

    using XJEditorViewportCommand = std::variant<
        XJSetPerformanceVisible,
        XJSetFrameGraphVisible,
        XJSetViewportRenderMode,
        XJSetViewportDepthRange,
        XJResetViewportPerformance>;

    struct XJEditorViewportRequest//视口请求
    {
        XJEditorViewportId Target = XJEditorViewportId::Game;
        XJEditorViewportCommand Command;
    };
}

#endif