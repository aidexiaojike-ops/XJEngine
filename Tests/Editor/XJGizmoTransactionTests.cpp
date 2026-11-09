#include "Asset/XJAssetRegistry.h"
#include "Controllers/XJEditorSceneController.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "Services/XJEditorSceneService.h"
#include "UI/XJEditorUIState.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdlib>
#include <string_view>

namespace
{
    using namespace XJ;

    void Check(bool value, std::string_view message)
    {
        if (!value)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    bool Near(
        const glm::dvec3& a,
        const glm::dvec3& b)
    {
        return glm::length(a - b) < 1.0e-4;
    }

    XJGizmoRenderedFrame MakeSource(
        XJScene& scene,
        XJEditorUIState& state,
        XJEditorTransformSpace space)
    {
        XJGizmoTransformSnapshot snapshot;

        Check(
            XJEditorSceneService::ReadGizmoTransform(
                scene,
                state.Selection.SelectedEntity,
                snapshot),
            "Read source Transform");

        state.TransformTools.Space = space;
        state.TransformTools.CanChangeSpace = true;

        XJGizmoRenderedFrame source;
        source.Valid = true;

        source.Epoch = state.GizmoEpoch;
        source.EntityId = snapshot.EntityId;

        source.SettingsRevision =
            state.TransformTools.SettingsRevision;

        source.Space = space;
        source.WorldPosition = snapshot.WorldPosition;

        source.Orientation =
            space == XJEditorTransformSpace::Local
                ? snapshot.WorldRotation
                : glm::dquat(1.0, 0.0, 0.0, 0.0);

        return source;
    }

    XJGizmoTransformSnapshot ReadPose(
        XJScene& scene,
        uint64_t id)
    {
        XJGizmoTransformSnapshot snapshot;

        Check(
            XJEditorSceneService::ReadGizmoTransform(
                scene, id, snapshot),
            "Read pose");

        return snapshot;
    }

    void TestCancelAndHistory()
    {
        XJScene scene;
        XJAssetRegistry registry;

        auto* entity =
            scene.CreateEntityWithTransform("Gizmo target");

        const uint64_t id =
            static_cast<uint64_t>(entity->XJGetUUID());

        Check(
            entity->GetComponent<XJTransformComponent>()
                .SetLocalTRSEuler(
                    {1, 2, 3},
                    {12, 24, 36},
                    {-2, 3, 4}) ==
                XJTransformStatus::Success,
            "Initial reflected pose");

        XJEditorSceneController controller;
        controller.SetScene(&scene);
        controller.SetAssetRegistry(&registry);

        XJEditorUIState state;
        state.Selection.SelectedEntity = id;

        const auto initial = ReadPose(scene, id);

        auto source =
            MakeSource(
                scene,
                state,
                XJEditorTransformSpace::Local);

        XJGizmoTransformSnapshot original;

        Check(
            controller.BeginTransformPreview(
                state,
                source,
                XJTransformGizmoHandle::RotateY,
                original),
            "Begin rotation preview");

        XJGizmoPreviewValue value;
        value.Operation = XJTransformGizmoOperation::Rotate;
        value.Space = XJEditorTransformSpace::Local;

        value.Rotation =
            glm::dquat(original.LocalRotation) *
            glm::angleAxis(
                glm::radians(45.0),
                glm::dvec3(0.0, 1.0, 0.0));

        Check(
            controller.PreviewTransform(state, value),
            "Preview rotation");

        Check(
            !controller.IsSceneDirty() &&
            !controller.CanUndo(),
            "Preview has no Dirty or history");

        Check(
            controller.EndTransformPreview(state, false),
            "Cancel rotation");

        const auto cancelled = ReadPose(scene, id);

        Check(
            Near(cancelled.LocalPosition, initial.LocalPosition) &&
            Near(cancelled.LocalScale, initial.LocalScale) &&
            glm::length(
                cancelled.WorldRotation *
                glm::dvec3(1, 0, 0) -
                initial.WorldRotation *
                glm::dvec3(1, 0, 0)) < 1.0e-5,
            "Cancel restores complete pose");

        Check(
            !controller.IsSceneDirty() &&
            !controller.CanUndo(),
            "Cancel preserves clean history");

        source =
            MakeSource(
                scene,
                state,
                XJEditorTransformSpace::Local);

        Check(
            controller.BeginTransformPreview(
                state,
                source,
                XJTransformGizmoHandle::ScaleX,
                original),
            "Begin scale preview");

        value = {};
        value.Operation = XJTransformGizmoOperation::Scale;
        value.Space = XJEditorTransformSpace::Local;
        value.Scale = glm::dvec3(original.LocalScale);

        value.Scale.x *= 2.0;

        Check(
            controller.PreviewTransform(state, value),
            "First scale preview");

        value.Scale.x =
            double(original.LocalScale.x) * 3.0;

        Check(
            controller.PreviewTransform(state, value),
            "Second scale preview");

        Check(
            controller.EndTransformPreview(state, true),
            "Commit scale");

        Check(
            controller.IsSceneDirty() &&
            controller.CanUndo(),
            "Commit creates history");

        Check(
            Near(ReadPose(scene, id).LocalScale, {-6, 3, 4}),
            "Local scaling preserves reflection");

        const uint64_t previousEpoch = state.GizmoEpoch;

        Check(controller.Undo(state), "Undo scale");

        // Undo 重建场景，必须重新通过 ID 查询。
        Check(
            Near(ReadPose(scene, id).LocalScale, {-2, 3, 4}),
            "One Undo restores entire drag");

        Check(
            !controller.CanUndo() &&
            controller.CanRedo(),
            "Only one history entry was created");

        Check(
            state.GizmoEpoch != previousEpoch &&
            !state.GizmoRendered.Valid &&
            !state.GizmoInteraction.Dragging,
            "Undo invalidates Gizmo input");

        Check(controller.Redo(state), "Redo scale");

        Check(
            Near(ReadPose(scene, id).LocalScale, {-6, 3, 4}),
            "Redo restores final scale");

        // 先 Undo 得到有效 Redo 链。
        Check(controller.Undo(state), "Undo before no-op");

        source =
            MakeSource(
                scene,
                state,
                XJEditorTransformSpace::Local);

        Check(
            controller.BeginTransformPreview(
                state,
                source,
                XJTransformGizmoHandle::ScaleX,
                original),
            "Begin no-op scale");

        value = {};
        value.Operation = XJTransformGizmoOperation::Scale;
        value.Space = XJEditorTransformSpace::Local;
        value.Scale = glm::dvec3(original.LocalScale);

        Check(
            controller.PreviewTransform(state, value),
            "No-op preview");

        Check(
            controller.EndTransformPreview(state, true),
            "End no-op preview");

        Check(
            !controller.CanUndo() &&
            controller.CanRedo(),
            "No-op preserves existing Redo chain");
    }

    void TestWorldScaleAndFailure()
    {
        XJScene scene;
        XJAssetRegistry registry;

        auto* parent =
            scene.CreateEntityWithTransform("Parent");

        auto* child =
            scene.CreateEntityWithTransform("Child");

        parent->XJAddChild(child);

        Check(
            parent->GetComponent<XJTransformComponent>()
                .SetLocalScale({2, 3, 4}) ==
                XJTransformStatus::Success,
            "Parent scale");

        Check(
            child->GetComponent<XJTransformComponent>()
                .SetLocalScale({-1, 2, -3}) ==
                XJTransformStatus::Success,
            "Child reflection");

        const uint64_t id =
            static_cast<uint64_t>(child->XJGetUUID());

        XJEditorSceneController controller;
        controller.SetScene(&scene);
        controller.SetAssetRegistry(&registry);

        XJEditorUIState state;
        state.Selection.SelectedEntity = id;

        auto source =
            MakeSource(
                scene,
                state,
                XJEditorTransformSpace::World);

        XJGizmoTransformSnapshot original;

        Check(
            controller.BeginTransformPreview(
                state,
                source,
                XJTransformGizmoHandle::ScaleY,
                original),
            "Begin World scale");

        XJGizmoPreviewValue value;
        value.Operation = XJTransformGizmoOperation::Scale;
        value.Space = XJEditorTransformSpace::World;
        value.Scale = original.WorldScale;
        value.Scale.y *= 2.0;

        Check(
            controller.PreviewTransform(state, value),
            "Preview World scale");

        const auto scaled = ReadPose(scene, id);

        Check(
            Near(scaled.WorldScale, value.Scale) &&
            scaled.LocalScale.x < 0.0f &&
            scaled.LocalScale.z < 0.0f,
            "World lossy target preserves local signs");

        Check(
            controller.EndTransformPreview(state, false),
            "Cancel World scale");

        Check(
            Near(
                ReadPose(scene, id).LocalScale,
                original.LocalScale),
            "Cancel restores reflected Local scale");

        source =
            MakeSource(
                scene,
                state,
                XJEditorTransformSpace::World);

        Check(
            controller.BeginTransformPreview(
                state,
                source,
                XJTransformGizmoHandle::ScaleX,
                original),
            "Begin invalid World scale test");

        value.Scale = original.WorldScale;
        value.Scale.x = -1.0;

        Check(
            !controller.PreviewTransform(state, value),
            "Negative World scale is rejected");

        Check(
            controller.EndTransformPreview(state, false),
            "Cancel rejected preview");

        Check(
            !controller.IsSceneDirty() &&
            !controller.CanUndo(),
            "Rejected and cancelled preview has no history");
    }
}

int main()
{
    TestCancelAndHistory();
    TestWorldScaleAndFailure();

    spdlog::info("XJGizmoTransactionTests passed.");
    return EXIT_SUCCESS;
}