#include "Controllers/XJEditorViewportDisplayController.h"
#include "UI/XJEditorUIState.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>
#include <limits>
#include <spdlog/spdlog.h>

namespace XJ
{
    namespace
    {
        struct ModeDescription//
        {
            XJEditorRenderMode EditorMode;
            XJRenderViewMode RenderMode;
            const char* Name;
        };

        constexpr ModeDescription Modes[] =
        {
            {XJEditorRenderMode::Final, XJRenderViewMode::Final, "FinalScreen"},
            {XJEditorRenderMode::BaseColor, XJRenderViewMode::BaseColor, "BaseColor"},
            {XJEditorRenderMode::LinearDepth, XJRenderViewMode::LinearDepth, "LinearDepth"},
            {XJEditorRenderMode::WorldNormal, XJRenderViewMode::WorldNormal, "WorldNormal"},
            {XJEditorRenderMode::Lighting, XJRenderViewMode::Lighting, "Lighting"}
        };
    }

    XJEditorViewportDisplayController::XJEditorViewportDisplayController(XJEditorUIState& state, IXJEditorViewportDisplayService& service)
        : mState(state), mService(service)
    {
        for (size_t index = 0; index < mViews.size(); ++index)
        {
            RefreshCapabilities(index);
            PublishSettings(index);
        }
    }
    std::optional<XJRenderViewMode> XJEditorViewportDisplayController::ToRenderMode(XJEditorRenderMode mode)//根据ui设置要渲染的模型
    {
        for(const auto& description : Modes)
        {
            if(description.EditorMode == mode)
                return description.RenderMode;
        }

        return std::nullopt;
    }

    void XJEditorViewportDisplayController::PublishSettings(size_t index)//设置窗口显示模型
    {
        auto& view = mState.ViewportDisplays[index];
        view.Settings = mViews[index].Settings;
        view.DepthRangeVisible = view.Settings.RenderMode == XJEditorRenderMode::LinearDepth;
    }

    bool XJEditorViewportDisplayController::ApplySettings(size_t index, const XJEditorViewportDisplaySettings& settings)
    {
        auto& view = mState.ViewportDisplays[index];
        const auto mode = ToRenderMode(settings.RenderMode);

        if (!mode || !std::isfinite(settings.DepthRange) || settings.DepthRange <= 0.0f)//设置无效
        {
            view.Error = "Display settings are invalid.";
            spdlog::warn("Viewport {} rejected invalid display settings: mode={}, depthRange={}",
                         index, static_cast<int>(settings.RenderMode), settings.DepthRange);
            return false;
        }

        const auto capabilities = mService.GetCapabilities(static_cast<XJEditorViewportId>(index));

        if (!capabilities.Available || !capabilities.Renderer.Supports(*mode))//当前不支持
        {
            view.Error = "The current viewport doesn't support the selected rendering mode.";
            spdlog::warn("Viewport {} rejected unsupported display mode {}.",
                         index, static_cast<int>(settings.RenderMode));
            return false;
        }

        const XJRenderViewOptions options{*mode, settings.DepthRange};

        if (!mService.ApplyViewOptions(static_cast<XJEditorViewportId>(index), options))//无法应用
        {
            view.Error = "Can't apply viewport display settings.";
            spdlog::error("Viewport {} failed to apply display settings.", index);
            return false;
        }

        mViews[index].Settings = settings;
        view.Error.clear();
        PublishSettings(index);
        spdlog::debug("Viewport {} applied display mode {}, depthRange={}",
                      index, static_cast<int>(settings.RenderMode), settings.DepthRange);
        return true;
    }

    void XJEditorViewportDisplayController::RefreshCapabilities(size_t index)
    {
        auto& state = mViews[index];
        const auto current = mService.GetCapabilities(static_cast<XJEditorViewportId>(index));

        if (state.CapabilitiesKnown &&
            state.Capabilities.Available == current.Available &&
            state.Capabilities.BindingEpoch == current.BindingEpoch &&
            state.Capabilities.Renderer.Revision == current.Renderer.Revision)
        {
            return;
        }

        state.CapabilitiesKnown = true;
        state.Capabilities = current;

        auto& choices = mState.ViewportDisplays[index].RenderModes;
        choices.clear();
        choices.reserve(std::size(Modes));

        for (const auto& description : Modes)
        {
            const bool enabled =
                current.Available &&
                current.Renderer.Supports(description.RenderMode);

            choices.push_back({
                description.EditorMode,
                description.Name,
                enabled,
                enabled ? "" : (
                    current.Available
                        ? "The current renderer doesn't support this mode yet."
                        : "Viewport is temporarily unavailable.")//不支持或者不可以
            });
        }

        // 新后端绑定时重新应用已有设置。
        if (current.Available)
        {
            auto settings = state.Settings;
            const auto mode = ToRenderMode(settings.RenderMode);

            if (!mode || !current.Renderer.Supports(*mode))
                settings.RenderMode = XJEditorRenderMode::Final;

            ApplySettings(index, settings);
        }
    }

    void XJEditorViewportDisplayController::ProcessRequests()//请求处理
    {
        for (size_t index = 0; index < mViews.size(); ++index)
            RefreshCapabilities(index);

        auto requests = std::move(mState.ViewportRequests);
        mState.ViewportRequests.clear();

        for (const auto& request : requests)
        {
            const size_t index = static_cast<size_t>(request.Target);
            if (index >= mViews.size())
            {
                spdlog::warn("Ignored display request for invalid viewport {}.", index);
                continue;
            }

            auto& state = mViews[index];

            std::visit([&](const auto& command)
            {
                using T = std::decay_t<decltype(command)>;

                if constexpr (std::is_same_v<T, XJSetPerformanceVisible>)
                {
                    state.Settings.ShowPerformance = command.Visible;
                    PublishSettings(index);
                }
                else if constexpr (std::is_same_v<T, XJSetFrameGraphVisible>)
                {
                    state.Settings.ShowFrameGraph = command.Visible;
                    PublishSettings(index);
                }
                else if constexpr (std::is_same_v<T, XJSetViewportRenderMode>)
                {
                    auto settings = state.Settings;
                    settings.RenderMode = command.Mode;
                    ApplySettings(index, settings);
                }
                else if constexpr (std::is_same_v<T, XJSetViewportDepthRange>)
                {
                    auto settings = state.Settings;
                    settings.DepthRange = command.Range;
                    ApplySettings(index, settings);
                }
                else if constexpr (std::is_same_v<T, XJResetViewportPerformance>)
                {
                    state.Head = 0;
                    state.Count = 0;
                    state.SincePublishMs = 0.0;
                    mState.ViewportDisplays[index].Performance = {};
                }
            }, request.Command);
        }
    }

    void XJEditorViewportDisplayController::PublishCompletedFrame(const XJEditorPerformanceSample& sample)//发布完整的帧率
    {
        if (sample.Frame.FrameId == 0 ||
            sample.Frame.FrameId <= mLastFrameId ||
            !std::isfinite(sample.Frame.WallMilliseconds) ||
            sample.Frame.WallMilliseconds <= 0.0 ||
            sample.Frame.WallMilliseconds > std::numeric_limits<float>::max())
        {
            return;
        }

        mLastFrameId = sample.Frame.FrameId;

        for (size_t index = 0; index < mViews.size(); ++index)
        {
            auto& state = mViews[index];
            state.History[state.Head] = sample;
            state.Head = (state.Head + 1) % state.History.size();
            state.Count = std::min(state.Count + 1, state.History.size());
            state.SincePublishMs += sample.Frame.WallMilliseconds;

            if (state.SincePublishMs >= 250.0 ||
                !mState.ViewportDisplays[index].Performance.Ready)
            {
                RebuildPerformance(index);
                state.SincePublishMs = 0.0;
            }
        }
    }

    void XJEditorViewportDisplayController::RebuildPerformance(size_t index)//性能
    {
        const auto& state = mViews[index];
        auto& view = mState.ViewportDisplays[index].Performance;
        view = {};

        if (state.Count == 0)
            return;

        const size_t capacity = state.History.size();
        const size_t first = (state.Head + capacity - state.Count) % capacity;
        size_t validScriptSamples = 0;

        for (size_t item = 0; item < state.Count; ++item)
        {
            const auto& sample = state.History[(first + item) % capacity];

            view.AverageFrameMs += sample.Frame.WallMilliseconds;
            view.MaximumFrameMs = std::max(
                view.MaximumFrameMs, sample.Frame.WallMilliseconds);

            if (sample.Scripts.FrameId == sample.Frame.FrameId &&
                std::isfinite(sample.Scripts.CpuMilliseconds) &&
                sample.Scripts.CpuMilliseconds >= 0.0 &&
                sample.Scripts.CpuMilliseconds <= std::numeric_limits<float>::max())
            {
                view.AverageScriptMs += sample.Scripts.CpuMilliseconds;
                ++validScriptSamples;
            }

            view.FrameHistory[item] =
                static_cast<float>(sample.Frame.WallMilliseconds);
        }

        view.HistoryCount = state.Count;
        view.AverageFrameMs /= static_cast<double>(state.Count);
        if (validScriptSamples != 0)
            view.AverageScriptMs /= static_cast<double>(validScriptSamples);
        view.FPS = 1000.0 / view.AverageFrameMs;

        const auto& latest =
            state.History[(state.Head + capacity - 1) % capacity];

        view.FrameId = latest.Frame.FrameId;

        if (latest.Scripts.FrameId == latest.Frame.FrameId &&
            std::isfinite(latest.Scripts.CpuMilliseconds) &&
            latest.Scripts.CpuMilliseconds >= 0.0 &&
            latest.Scripts.CpuMilliseconds <= std::numeric_limits<float>::max())
        {
            view.ScriptStatisticsAvailable = true;
            view.ActiveScripts = latest.Scripts.ActiveInstances;
        }

        const auto& render = latest.Views[index];

        if (render.FrameId == latest.Frame.FrameId)
        {
            view.RenderStatisticsAvailable = true;
            view.CommandsRecorded = render.CommandsRecorded;
            view.DrawCalls = render.DrawCalls;
            view.ParameterUploads = render.MaterialParameterUploads;
            view.ResourceUploads = render.MaterialResourceUploads;
        }

        view.Ready = true;
    }


}
