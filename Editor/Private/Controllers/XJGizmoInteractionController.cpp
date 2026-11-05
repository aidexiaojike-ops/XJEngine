#include "Controllers/XJGizmoInteractionController.h"

#include "Controllers/XJEditorSceneController.h"
#include "Tools/XJTransformGizmoInteractionMath.h"
#include "UI/XJEditorUIState.h"

namespace XJ
{
    namespace
    {
        bool SameMatrix(const glm::mat4& a, const glm::mat4& b)
        {
            for (int column = 0; column < 4; ++column)
            {
                if (!glm::all(
                        glm::equal(a[column], b[column])))
                {
                    return false;
                }
            }

            return true;
        }
    }

    void XJGizmoInteractionController::Cancel(XJEditorUIState& state, XJEditorSceneController& sceneController)
    {
        sceneController.CancelMovePreview();

        mDragging = false;
        mStartFrame = {};
        mConstraint = {};

        state.GizmoInteraction = {};
        state.GizmoPointer = {};
        state.GizmoRendered = {};

        ++state.GizmoEpoch;
    }

    bool XJGizmoInteractionController::Process(XJEditorUIState& state, XJEditorSceneController& sceneController)
    {
        using Handle = XJTransformGizmoHandle;

        const auto input = state.GizmoPointer;
        state.GizmoPointer = {};

        auto& interaction = state.GizmoInteraction;

        interaction.Hovered = Handle::None;

        const bool ownedAtStart = mDragging;

        const auto finish =
            [&](bool commit)
        {
            sceneController.EndMovePreview(state, commit);

            mDragging = false;

            interaction.Dragging = false;
            interaction.Active = Handle::None;
        };

        const bool valid =
            input.Pending &&
            !input.Cancel &&
            input.Frame.Valid &&
            input.Frame.Epoch == state.GizmoEpoch &&
            input.Frame.EntityId ==
                state.Selection.SelectedEntity &&
            input.Frame.Space == state.TransformTools.Space &&
            input.Frame.SettingsRevision ==
                state.TransformTools.SettingsRevision &&
            state.TransformTools.CanChangeSpace &&
            !state.WorkspaceReadOnly &&
            !state.RuntimeInspection;

        if (!valid)
        {
            if (mDragging)
                finish(false);

            return ownedAtStart;
        }

        if (mDragging)
        {
            // 改变图像区域或相机后，不继续使用旧约束。
            const bool unchanged =
                input.Frame.EntityId == mStartFrame.EntityId &&
                input.Frame.Epoch == mStartFrame.Epoch &&
                input.Frame.Width == mStartFrame.Width &&
                input.Frame.Height == mStartFrame.Height &&
                glm::all(glm::equal(
                    input.ImageMin, mImageMin)) &&
                glm::all(glm::equal(
                    input.ImageSize, mImageSize)) &&
                SameMatrix(
                    input.Frame.View,
                    mStartFrame.View) &&
                SameMatrix(
                    input.Frame.Projection,
                    mStartFrame.Projection);

            if (!unchanged)
            {
                finish(false);
                return true;
            }

            if (!input.Down && !input.Released)
            {
                // 失去鼠标按钮状态，但没有正常收到释放。
                finish(false);
                return true;
            }

            glm::dvec3 target;

            if (CalculateGizmoMoveTarget(
                    mConstraint,
                    input.UV,
                    target))
            {
                if (!sceneController.PreviewMovePosition(
                        state,
                        target))
                {
                    finish(false);
                    return true;
                }
            }

            // 射线与平面条件不好时暂时保持最后有效位置；
            // 恢复后仍从按下快照计算，不累计误差。
            if (input.Released)
                finish(true);

            return true;
        }

        if (!input.Inside)
            return false;

        const Handle hovered =
            PickTransformGizmo(
                input.Frame,
                input.UV,
                input.ImageSize);

        interaction.Hovered = hovered;

        if (!input.Pressed || hovered == Handle::None)
            return false;

        interaction.Error.clear();

        if (GetGizmoOperation(hovered) !=
            XJTransformGizmoOperation::Move)
        {
            // 本步仅移动。
            // 旋转和缩放点击仍被消费，不能穿透选择模型。
            return true;
        }

        XJGizmoMoveConstraint constraint;

        if (!BeginGizmoMoveConstraint(
                input.Frame,
                hovered,
                input.UV,
                constraint))
        {
            interaction.Error =
                "The move axis is too close to the view "
                "direction. Change the viewing angle.";

            return true;
        }

        if (!sceneController.BeginMovePreview(
                state,
                input.Frame.EntityId,
                input.Frame.WorldPosition))
        {
            interaction.Error =
                "The move preview could not be started.";

            return true;
        }

        mConstraint = constraint;
        mStartFrame = input.Frame;

        mImageMin = input.ImageMin;
        mImageSize = input.ImageSize;

        mDragging = true;

        interaction.Dragging = true;
        interaction.Active = hovered;
        interaction.Hovered = hovered;

        // 兼容极短的“同一 UI 帧按下并释放”。
        if (input.Released)
            finish(true);

        return true;
    }
}
