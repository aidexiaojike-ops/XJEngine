#ifndef XJ_EDITOR_VIEWPORT_DISPLAY_CONTROLLER_H
#define XJ_EDITOR_VIEWPORT_DISPLAY_CONTROLLER_H

#include "Services/XJEditorViewportDisplayService.h"
#include "UI/XJEditorViewportDisplayViewModel.h"

#include <array>
#include <optional>
//视口显示控制器
namespace XJ
{

    struct XJEditorUIState;

    struct XJEditorPerformanceSample//性能采样
    {
        XJCompletedFrameStatistics Frame;
        XJScriptStatistics Scripts;
        std::array<XJRenderStatistics, XJ_EDITOR_VIEWPORT_COUNT> Views{};
    };

    class XJEditorViewportDisplayController//视口显示控制器
    {
        public:
            XJEditorViewportDisplayController(XJEditorUIState& state, IXJEditorViewportDisplayService& service);

            void ProcessRequests();//
            void PublishCompletedFrame(const XJEditorPerformanceSample& sample);

        private:
            struct ViewState
            {
                XJEditorViewportDisplaySettings Settings;

                bool CapabilitiesKnown = false;
                XJEditorViewportCapabilities Capabilities;

                std::array<
                    XJEditorPerformanceSample,
                    XJ_EDITOR_PERFORMANCE_HISTORY> History{};//

                size_t Head = 0;
                size_t Count = 0;
                double SincePublishMs = 0.0;
            };

            static std::optional<XJRenderViewMode> ToRenderMode(XJEditorRenderMode mode);

            void RefreshCapabilities(size_t index);//
            void PublishSettings(size_t index);
            void RebuildPerformance(size_t index);
            bool ApplySettings(
                size_t index,
                const XJEditorViewportDisplaySettings& settings);

            XJEditorUIState& mState;
            IXJEditorViewportDisplayService& mService;
            std::array<ViewState, XJ_EDITOR_VIEWPORT_COUNT> mViews{};
            uint64_t mLastFrameId = 0;
    };
}

#endif