#include "UI/XJViewportDisplayWidgets.h"
#include "UI/XJEditorUIState.h"

#include <algorithm>
#include <cstdio>

namespace XJ
{
    bool XJViewportDisplayWidgets::DrawMenu(XJEditorUIState& state, XJEditorViewportId viewport)
    {
        const size_t index = static_cast<size_t>(viewport);
        if (index >= state.ViewportDisplays.size())
            return false;

        const auto& view = state.ViewportDisplays[index];

        ImGui::PushID(static_cast<int>(index));
        ImGui::SetNextItemWidth(112.0f);

        const bool open = ImGui::BeginCombo("##Display", "Display");//显示

        if (open)
        {
            bool performance = view.Settings.ShowPerformance;
            bool graph = view.Settings.ShowFrameGraph;

            if (ImGui::Checkbox("Performance stats", &performance))//性能统计
            {
                state.ViewportRequests.push_back({
                    viewport, XJSetPerformanceVisible{performance}
                });
            }

            if (ImGui::Checkbox("Frame Time Curve", &graph))//帧时间曲线
            {
                state.ViewportRequests.push_back({
                    viewport, XJSetFrameGraphVisible{graph}
                });
            }

            if (ImGui::Button("Reset Stats"))//重置统计
            {
                state.ViewportRequests.push_back({
                    viewport, XJResetViewportPerformance{}
                });
            }

            ImGui::SeparatorText("Rendering Mode");//渲染模式

            for (const auto& choice : view.RenderModes)
            {
                ImGui::BeginDisabled(!choice.bEnabled);

                if (ImGui::Selectable(
                        choice.Name.c_str(),
                        view.Settings.RenderMode == choice.Mode))
                {
                    state.ViewportRequests.push_back({
                        viewport, XJSetViewportRenderMode{choice.Mode}
                    });
                }

                ImGui::EndDisabled();

                if (!choice.bEnabled &&
                    ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip("%s", choice.DisabledReason.c_str());
                }
            }

            if (view.DepthRangeVisible)//深度范围
            {
                float range = view.Settings.DepthRange;

                if (ImGui::DragFloat(
                        "Depth range", &range, 0.1f,
                        0.1f, 10000.0f, "%.1f",
                        ImGuiSliderFlags_AlwaysClamp))
                {
                    state.ViewportRequests.push_back({
                        viewport, XJSetViewportDepthRange{range}
                    });
                }
            }

            if (!view.Error.empty())
                ImGui::TextWrapped("%s", view.Error.c_str());

            ImGui::EndCombo();
        }

        ImGui::PopID();
        return open;
    }

    void XJViewportDisplayWidgets::DrawPerformance(const XJEditorUIState& state, XJEditorViewportId viewport, const ImVec2& imagePosition, const ImVec2& imageSize)
    {
        const size_t index = static_cast<size_t>(viewport);
        if (index >= state.ViewportDisplays.size())
            return;

        const auto& view = state.ViewportDisplays[index];

        if (!view.Settings.ShowPerformance &&
            !view.Settings.ShowFrameGraph)
        {
            return;
        }

        const float width = std::min(320.0f, imageSize.x - 16.0f);
        const float contentHeight =
            (view.Settings.ShowPerformance ? 8.0f * ImGui::GetTextLineHeightWithSpacing() : 0.0f) +
            (view.Settings.ShowFrameGraph ? 80.0f : 0.0f) + 16.0f;
        const float height = std::min(
            contentHeight,
            imageSize.y - 16.0f);

        if (width < 140.0f || height < 40.0f)
            return;

        char title[64];
        std::snprintf(title, sizeof(title),
                      "##ViewportPerformance_%zu", index);

        ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID);
        ImGui::SetNextWindowPos(
            ImVec2(imagePosition.x + 8.0f, imagePosition.y + 8.0f));
        ImGui::SetNextWindowSize(ImVec2(width, height));
        ImGui::SetNextWindowBgAlpha(0.78f);

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::Begin(title, nullptr, flags))
        {
            const auto& performance = view.Performance;

            if (!performance.Ready)
            {
                ImGui::TextUnformatted("Waiting for completed frame...");
            }
            else
            {
                if (view.Settings.ShowPerformance)
                {
                    ImGui::Text("Loop FPS: %.1f", performance.FPS);
                    ImGui::Text(
                        "Frame avg/max: %.2f / %.2f ms",
                        performance.AverageFrameMs,
                        performance.MaximumFrameMs);
                    if (performance.ScriptStatisticsAvailable)
                        ImGui::Text("Runtime script CPU: %.3f ms", performance.AverageScriptMs);
                    else
                        ImGui::TextDisabled("Runtime script statistics unavailable");
                    ImGui::Text(
                        "Active scripts: %u",
                        performance.ActiveScripts);
                    ImGui::Text(
                        "Draw calls: %llu",
                        static_cast<unsigned long long>(
                            performance.DrawCalls));
                    ImGui::Text(
                        "Material uploads: %llu / %llu",
                        static_cast<unsigned long long>(
                            performance.ParameterUploads),
                        static_cast<unsigned long long>(
                            performance.ResourceUploads));
                    ImGui::Text(
                        "Frame: %llu",
                        static_cast<unsigned long long>(
                            performance.FrameId));

                    if (!performance.RenderStatisticsAvailable)
                        ImGui::TextDisabled("View statistics unavailable");
                    else if (!performance.CommandsRecorded)
                        ImGui::TextDisabled("No view commands this frame");
                }

                if (view.Settings.ShowFrameGraph &&
                    performance.HistoryCount != 0)
                {
                    ImGui::PlotLines(
                        "##FrameTime",
                        performance.FrameHistory.data(),
                        static_cast<int>(performance.HistoryCount),
                        0,
                        "Frame ms",
                        0.0f,
                        static_cast<float>(std::max(
                            33.333, performance.MaximumFrameMs)),
                        ImVec2(-1.0f, 60.0f));
                }
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }
}
