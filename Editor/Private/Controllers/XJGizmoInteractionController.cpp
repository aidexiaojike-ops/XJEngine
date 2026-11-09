#include "Controllers/XJGizmoInteractionController.h"

#include "Controllers/XJEditorSceneController.h"
#include "Tools/XJTransformGizmoInteractionMath.h"
#include "UI/XJEditorUIState.h"

#include <glm/gtc/constants.hpp>

#include <cmath>

namespace XJ
{
    namespace
    {
        bool SameMatrix(const glm::mat4& a, const glm::mat4& b)
        {
            for (int column = 0; column < 4; ++column)
            {
                if (!glm::all(glm::equal(a[column], b[column])))
                    return false;
                
            }

            return true;
        }

        int AxisIndex(XJTransformGizmoHandle handle)
        {
            switch (GetGizmoAxis(handle))
            {
                case XJTransformGizmoAxis::X: return 0;
                case XJTransformGizmoAxis::Y: return 1;
                case XJTransformGizmoAxis::Z: return 2;
            }

            return 0;
        }

        XJTransformGizmoHandle MoveHandleForAxis(XJTransformGizmoHandle handle)
        {
            switch (GetGizmoAxis(handle))
            {
                case XJTransformGizmoAxis::X:
                    return XJTransformGizmoHandle::MoveX;

                case XJTransformGizmoAxis::Y:
                    return XJTransformGizmoHandle::MoveY;

                case XJTransformGizmoAxis::Z:
                    return XJTransformGizmoHandle::MoveZ;
            }

            return XJTransformGizmoHandle::None;
        }
    }

    void XJGizmoInteractionController::Cancel(XJEditorUIState& state, XJEditorSceneController& sceneController)
    {
        const bool restored =
            sceneController.CancelTransformPreview();

        mDragging = false;
        mHandle = XJTransformGizmoHandle::None;

        mStartFrame = {};
        mOriginal = {};
        mLinearConstraint = {};
        mRotateConstraint = {};

        state.GizmoInteraction = {};
        state.GizmoPointer = {};
        state.GizmoRendered = {};
        state.TransformGizmo = {};

        ++state.GizmoEpoch;

        if (!restored)
        {
            state.GizmoInteraction.Error =
                "The Gizmo preview could not be fully restored.";
        }
    }

    bool XJGizmoInteractionController::BuildTarget(const XJGizmoPointerInput& input, XJGizmoPreviewValue& value)
    {
        using Operation = XJTransformGizmoOperation;
        using Space = XJEditorTransformSpace;

        value = {};
        value.Operation = GetGizmoOperation(mHandle);
        value.Space = mStartFrame.Space;

        switch (value.Operation)
        {
            case Operation::Move:
            {
                value.Space = Space::World;

                return CalculateGizmoMoveTarget(
                    mLinearConstraint,
                    input.UV,
                    value.Position);
            }

            case Operation::Rotate:
            {
                if (!SampleGizmoRotateConstraint(
                        mRotateConstraint,
                        input.UV))
                {
                    return false;
                }

                // 避免转很多圈之后把巨大角度传给 sin/cos。
                // 累计角度仍保留，只有生成四元数时取余。
                const double angle =
                    std::remainder(
                        mRotateConstraint.TotalAngle,
                        glm::two_pi<double>());

                if (value.Space == Space::Local)
                {
                    const glm::dquat delta =
                        glm::angleAxis(
                            angle,
                            GetGizmoUnitAxis(mHandle));

                    value.Rotation =
                        glm::dquat(mOriginal.LocalRotation) *
                        delta;
                }
                else
                {
                    const glm::dquat delta =
                        glm::angleAxis(
                            angle,
                            mRotateConstraint.Axis);

                    value.Rotation =
                        delta * mOriginal.WorldRotation;
                }

                value.Rotation =
                    glm::normalize(value.Rotation);

                return true;
            }

            case Operation::Scale:
            {
                double factor = 1.0;

                if (!CalculateGizmoScaleFactor(
                        mLinearConstraint,
                        input.UV,
                        mScaleReferenceLength,
                        factor))
                {
                    return false;
                }

                value.Scale =
                    value.Space == Space::Local
                        ? glm::dvec3(mOriginal.LocalScale)
                        : mOriginal.WorldScale;

                const int axis = AxisIndex(mHandle);

                value.Scale[axis] *= factor;

                return
                    std::isfinite(value.Scale.x) &&
                    std::isfinite(value.Scale.y) &&
                    std::isfinite(value.Scale.z);
            }

            default:
                return false;
        }
    }

    bool XJGizmoInteractionController::Process(XJEditorUIState& state, XJEditorSceneController& sceneController)
    {
        using Handle = XJTransformGizmoHandle;
        using Operation = XJTransformGizmoOperation;

        const auto input = state.GizmoPointer;
        state.GizmoPointer = {};

        auto& interaction = state.GizmoInteraction;
        interaction.Hovered = Handle::None;

        const bool ownedAtStart = mDragging;

        const auto finish =
            [&](bool commit)
        {
            bool succeeded = true;

            if (sceneController.IsTransformPreviewActive())
            {
                succeeded =
                    sceneController.EndTransformPreview(
                        state,
                        commit);
            }

            mDragging = false;
            mHandle = Handle::None;

            interaction.Dragging = false;
            interaction.Active = Handle::None;

            if (!succeeded && interaction.Error.empty())
            {
                interaction.Error =
                    "The Gizmo transaction could not be completed.";
            }
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
            const bool unchanged =
                sceneController.IsTransformPreviewActive() &&
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
                finish(false);
                return true;
            }

            XJGizmoPreviewValue value;

            if (BuildTarget(input, value))
            {
                if (!sceneController.PreviewTransform(
                        state,
                        value))
                {
                    finish(false);
                    return true;
                }
            }

            // 临时遇到几何退化时冻结最后有效结果。
            // 松开仍可提交最后有效结果。
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

        const Operation operation =
            GetGizmoOperation(hovered);

        XJGizmoMoveConstraint linearConstraint;
        XJGizmoRotateConstraint rotateConstraint;

        bool prepared = false;

        if (operation == Operation::Rotate)
        {
            prepared =
                BeginGizmoRotateConstraint(
                    input.Frame,
                    hovered,
                    input.UV,
                    rotateConstraint);
        }
        else if (operation == Operation::Move ||
                 operation == Operation::Scale)
        {
            // 缩放与移动复用同轴的线性约束。
            prepared =
                BeginGizmoMoveConstraint(
                    input.Frame,
                    MoveHandleForAxis(hovered),
                    input.UV,
                    linearConstraint);
        }

        if (!prepared)
        {
            interaction.Error =
                "The handle is poorly aligned with the "
                "view. Change the viewing angle.";

            return true;
        }

        const double worldScale =
            glm::length(
                glm::dvec3(input.Frame.Model[0]));

        const double scaleReference =
            worldScale *
            double(XJTransformGizmoMetrics::ScaleBoxCenter);

        if (!std::isfinite(scaleReference) ||
            scaleReference <= 1.0e-12)
        {
            interaction.Error =
                "The Gizmo scale is invalid.";

            return true;
        }

        XJGizmoTransformSnapshot original;

        if (!sceneController.BeginTransformPreview(
                state,
                input.Frame,
                hovered,
                original))
        {
            interaction.Error =
                "The Transform preview could not be started.";

            return true;
        }

        mHandle = hovered;
        mStartFrame = input.Frame;
        mOriginal = original;

        mLinearConstraint = linearConstraint;
        mRotateConstraint = rotateConstraint;

        mScaleReferenceLength = scaleReference;

        mImageMin = input.ImageMin;
        mImageSize = input.ImageSize;

        mDragging = true;

        interaction.Dragging = true;
        interaction.Active = hovered;
        interaction.Hovered = hovered;

        if (input.Released)
            finish(true);

        return true;
    }
}