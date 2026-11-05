#include "UI/XJTransformToolWidgets.h"
#include "UI/XJEditorUIState.h"

#include <imgui.h>

namespace XJ
{
    void XJTransformToolWidgets::DrawSpaceButton(XJEditorUIState& state)
    {
        const auto& view = state.TransformTools;

        const bool isLocal = view.Space == XJEditorTransformSpace::Local;

        ImGui::PushID("TransformTools");

        ImGui::BeginDisabled(!view.CanChangeSpace || state.GizmoInteraction.Dragging);

        const char* label = isLocal
            ? "Local##TransformSpace"
            : "World##TransformSpace";

        if (ImGui::Button(label, ImVec2(90.0f, 0.0f)))
        {
            const auto requestedSpace = isLocal
                ? XJEditorTransformSpace::World
                : XJEditorTransformSpace::Local;

            state.TransformToolRequests.emplace_back(
                XJSetTransformSpace{requestedSpace});
        }

        ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::BeginTooltip();

            if (state.GizmoInteraction.Dragging)
            {
                ImGui::TextUnformatted(
                    "Release or cancel the Gizmo drag "
                    "before changing coordinate space.");
            }

            if (!view.CanChangeSpace)
            {
                ImGui::TextUnformatted(
                    "Transform tools are available "
                    "in Edit mode with an active scene.");
            }
            else
            {
                ImGui::TextUnformatted(
                    isLocal
                        ? "Local axes follow the object's "
                          "world rotation."
                        : "World axes align with the "
                          "global XYZ axes.");

                ImGui::TextUnformatted(
                    "Click to switch coordinate space.");
            }

            ImGui::EndTooltip();
        }

        ImGui::PopID();
    }
}