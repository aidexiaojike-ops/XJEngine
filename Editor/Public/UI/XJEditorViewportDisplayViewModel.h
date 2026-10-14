#ifndef XJ_EDITOR_VIEWPORT_DISPLAY_VIEW_MODEL_H
#define XJ_EDITOR_VIEWPORT_DISPLAY_VIEW_MODEL_H

#include "Viewport/XJEditorViewportTypes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace XJ
{
    inline constexpr size_t XJ_EDITOR_PERFORMANCE_HISTORY = 128;

    struct XJEditorViewportDisplaySettings//视口显示设置
    {
        bool ShowPerformance = false;
        bool ShowFrameGraph = false;

        XJEditorRenderMode RenderMode = XJEditorRenderMode::Final;
        float DepthRange = 10.0f;
    };

    struct XJEditorRenderModeChoice//渲染模式选择
    {
        XJEditorRenderMode Mode = XJEditorRenderMode::Final;
        std::string Name;
        bool bEnabled = false;
        std::string DisabledReason;
    };

    struct XJEditorPerformanceViewModel//性能视图模型
    {
        bool Ready = false;
        uint64_t FrameId = 0;

        double FPS = 0.0;
        double AverageFrameMs = 0.0;
        double MaximumFrameMs = 0.0;
        double AverageScriptMs = 0.0;

        uint32_t ActiveScripts = 0;
        bool ScriptStatisticsAvailable = false;
        bool RenderStatisticsAvailable = false;
        bool CommandsRecorded = false;
        uint64_t DrawCalls = 0;
        uint64_t ParameterUploads = 0;
        uint64_t ResourceUploads = 0;

        std::array<float, XJ_EDITOR_PERFORMANCE_HISTORY> FrameHistory{};
        size_t HistoryCount = 0;
    };

    struct XJEditorViewportDisplayViewModel//视口显示视图模型
    {
        XJEditorViewportDisplaySettings Settings;
        std::vector<XJEditorRenderModeChoice> RenderModes;
        XJEditorPerformanceViewModel Performance;

        bool DepthRangeVisible = false;
        std::string Error;
    };
    

}

#endif
