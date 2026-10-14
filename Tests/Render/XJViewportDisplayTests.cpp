#include "Controllers/XJEditorViewportDisplayController.h"
#include "UI/XJEditorUIState.h"

#include <spdlog/spdlog.h>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace
{
    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    class FakeService final : public XJ::IXJEditorViewportDisplayService
    {
    public:
        std::array<XJ::XJEditorViewportCapabilities, XJ::XJ_EDITOR_VIEWPORT_COUNT> Capabilities{};
        std::array<XJ::XJRenderViewOptions, XJ::XJ_EDITOR_VIEWPORT_COUNT> Options{};
        std::array<unsigned, XJ::XJ_EDITOR_VIEWPORT_COUNT> Applications{};
        bool FailApply = false;

        FakeService()
        {
            for (auto& capabilities : Capabilities)
            {
                capabilities.Available = true;
                capabilities.Renderer.Supported[static_cast<size_t>(XJ::XJRenderViewMode::BaseColor)] = true;
            }
        }

        void BeginStatisticsFrame(uint64_t) override {}
        XJ::XJRenderStatistics ReadStatistics(XJ::XJEditorViewportId) const override { return {}; }
        XJ::XJEditorViewportCapabilities GetCapabilities(XJ::XJEditorViewportId viewport) const override
        {
            return Capabilities.at(static_cast<size_t>(viewport));
        }
        bool ApplyViewOptions(XJ::XJEditorViewportId viewport, const XJ::XJRenderViewOptions& options) override
        {
            const auto index = static_cast<size_t>(viewport);
            if (FailApply || !Capabilities.at(index).Available ||
                !Capabilities.at(index).Renderer.Supports(options.Mode))
                return false;
            ++Applications.at(index);
            Options.at(index) = options;
            return true;
        }
    };

    XJ::XJEditorPerformanceSample MakeSample(uint64_t frameId, double frameMs = 250.0)
    {
        XJ::XJEditorPerformanceSample sample;
        sample.Frame = {frameId, frameMs};
        sample.Scripts = {frameId, 2.0, 3};
        for (size_t index = 0; index < sample.Views.size(); ++index)
        {
            sample.Views[index].FrameId = frameId;
            sample.Views[index].CommandsRecorded = true;
            sample.Views[index].DrawCalls = 10 + index;
            sample.Views[index].MaterialParameterUploads = index;
        }
        return sample;
    }

    void TestRequests()
    {
        XJ::XJEditorUIState state;
        FakeService service;
        XJ::XJEditorViewportDisplayController controller(state, service);
        constexpr auto game = XJ::XJEditorViewportId::Game;
        constexpr size_t gameIndex = static_cast<size_t>(game);
        constexpr size_t sceneIndex = static_cast<size_t>(XJ::XJEditorViewportId::Scene);
        const auto applications = service.Applications;

        state.ViewportRequests.push_back({game, XJ::XJSetPerformanceVisible{true}});
        state.ViewportRequests.push_back({game, XJ::XJSetFrameGraphVisible{true}});
        controller.ProcessRequests();
        Check(service.Applications == applications, "visibility changes never touch the renderer");
        Check(state.ViewportDisplays[gameIndex].Settings.ShowPerformance &&
              !state.ViewportDisplays[sceneIndex].Settings.ShowPerformance,
              "viewport preferences are independent");

        state.ViewportRequests.push_back({game, XJ::XJSetViewportRenderMode{XJ::XJEditorRenderMode::BaseColor}});
        controller.ProcessRequests();
        Check(service.Options[gameIndex].Mode == XJ::XJRenderViewMode::BaseColor &&
              service.Options[sceneIndex].Mode == XJ::XJRenderViewMode::Final,
              "mode requests route only to their target renderer");

        state.ViewportRequests.push_back({game, XJ::XJSetViewportRenderMode{XJ::XJEditorRenderMode::LinearDepth}});
        state.ViewportRequests.push_back({game, XJ::XJSetViewportDepthRange{std::numeric_limits<float>::quiet_NaN()}});
        state.ViewportRequests.push_back({static_cast<XJ::XJEditorViewportId>(99), XJ::XJSetPerformanceVisible{true}});
        controller.ProcessRequests();
        Check(!state.ViewportDisplays[gameIndex].Error.empty() &&
              service.Options[gameIndex].Mode == XJ::XJRenderViewMode::BaseColor,
              "invalid and unsupported requests retain the accepted settings");
        Check(state.ViewportRequests.empty(), "requests are consumed exactly once");

        service.FailApply = true;
        state.ViewportRequests.push_back({game, XJ::XJSetViewportRenderMode{XJ::XJEditorRenderMode::Final}});
        controller.ProcessRequests();
        Check(state.ViewportDisplays[gameIndex].Settings.RenderMode == XJ::XJEditorRenderMode::BaseColor &&
              !state.ViewportDisplays[gameIndex].Error.empty(),
              "backend failure never publishes an unaccepted setting");
        service.FailApply = false;

        service.Capabilities[gameIndex].Renderer.Supported[static_cast<size_t>(XJ::XJRenderViewMode::BaseColor)] = false;
        ++service.Capabilities[gameIndex].Renderer.Revision;
        controller.ProcessRequests();
        Check(state.ViewportDisplays[gameIndex].Settings.RenderMode == XJ::XJEditorRenderMode::Final,
              "removed capability falls back to Final");
        service.Capabilities[gameIndex].Available = false;
        controller.ProcessRequests();
        for (const auto& choice : state.ViewportDisplays[gameIndex].RenderModes)
            Check(!choice.bEnabled, "unavailable backend disables all mode choices");
        service.Capabilities[gameIndex].Available = true;
        ++service.Capabilities[gameIndex].BindingEpoch;
        controller.ProcessRequests();
        Check(state.ViewportDisplays[gameIndex].Error.empty(), "rebind applies settings and clears errors");
    }

    void TestFrames()
    {
        XJ::XJEditorUIState state;
        FakeService service;
        XJ::XJEditorViewportDisplayController controller(state, service);
        constexpr size_t game = static_cast<size_t>(XJ::XJEditorViewportId::Game);
        constexpr uint64_t firstFrame = uint64_t{1} << 40;

        controller.PublishCompletedFrame(MakeSample(firstFrame));
        const auto& first = state.ViewportDisplays[game].Performance;
        Check(first.Ready && first.FrameId == firstFrame && first.DrawCalls == 11 &&
              first.ParameterUploads == 1 && first.ScriptStatisticsAvailable,
              "64-bit frame identity and per-viewport counters reach the view model");
        controller.PublishCompletedFrame(MakeSample(firstFrame));
        controller.PublishCompletedFrame(MakeSample(firstFrame + 1, std::numeric_limits<double>::infinity()));
        Check(state.ViewportDisplays[game].Performance.HistoryCount == 1,
              "duplicate and invalid frames are rejected");

        auto mismatch = MakeSample(firstFrame + 2);
        mismatch.Scripts.FrameId = 0;
        mismatch.Views[game].FrameId = 0;
        controller.PublishCompletedFrame(mismatch);
        Check(!state.ViewportDisplays[game].Performance.ScriptStatisticsAvailable &&
              !state.ViewportDisplays[game].Performance.RenderStatisticsAvailable &&
              state.ViewportDisplays[game].Performance.AverageScriptMs == 2.0,
              "mismatched sources are unavailable and do not bias valid script averages");

        for (uint64_t index = 3; index < 160; ++index)
            controller.PublishCompletedFrame(MakeSample(firstFrame + index, static_cast<double>(index)));
        const auto& history = state.ViewportDisplays[game].Performance;
        Check(history.HistoryCount == XJ::XJ_EDITOR_PERFORMANCE_HISTORY &&
              history.FrameHistory[0] < history.FrameHistory[history.HistoryCount - 1],
              "history is bounded and chronological");

        state.ViewportRequests.push_back({XJ::XJEditorViewportId::Game, XJ::XJResetViewportPerformance{}});
        controller.ProcessRequests();
        Check(!state.ViewportDisplays[game].Performance.Ready &&
              state.ViewportDisplays[0].Performance.Ready,
              "reset affects only the selected viewport");
    }

    void TestScriptRecorder()
    {
        XJ::XJScriptStatisticsRecorder recorder;
        constexpr uint64_t frameId = uint64_t{1} << 40;
        recorder.BeginFrame(frameId);
        {
            XJ::XJScriptStatisticsRecorder::Scope outer(recorder);
            {
                XJ::XJScriptStatisticsRecorder::Scope inner(recorder);
                bool rejected = false;
                try { recorder.BeginFrame(frameId + 1); }
                catch (const std::logic_error&) { rejected = true; }
                Check(rejected, "active nested scopes reject reset using a standard exception");
            }
            Check(recorder.GetSnapshot(4).CpuMilliseconds == 0.0,
                  "inner scopes never publish or double count time");
        }
        Check(recorder.GetSnapshot(4).FrameId == frameId,
              "rejected reset preserves the original frame");
        recorder.BeginFrame(frameId + 1);
        Check(recorder.GetSnapshot(0).CpuMilliseconds == 0.0,
              "scope unwind restores depth and the next frame resets time");
    }
}

int main()
{
    TestRequests();
    TestFrames();
    TestScriptRecorder();
    spdlog::info("Viewport display and performance tests passed.");
    return EXIT_SUCCESS;
}
