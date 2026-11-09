#ifndef XJ_GIZMO_INTERACTION_CONTROLLER_H
#define XJ_GIZMO_INTERACTION_CONTROLLER_H

#include "Tools/XJTransformGizmoInteractionTypes.h"

namespace XJ
{
    struct XJEditorUIState;
    class XJEditorSceneController;

    class XJGizmoInteractionController
    {
        public:
            // 返回本帧是否被 Gizmo 手势占用。
            bool Process(XJEditorUIState& state, XJEditorSceneController& sceneController);

            // 生命周期变化前调用。
            void Cancel(XJEditorUIState& state, XJEditorSceneController& sceneController);

        private:
            bool BuildTarget(const XJGizmoPointerInput& input, XJGizmoPreviewValue& value);

            bool mDragging = false;

            XJTransformGizmoHandle mHandle = XJTransformGizmoHandle::None;

            XJGizmoRenderedFrame mStartFrame;
            XJGizmoTransformSnapshot mOriginal;

            XJGizmoMoveConstraint mLinearConstraint;
            XJGizmoRotateConstraint mRotateConstraint;

            double mScaleReferenceLength = 1.0;

            glm::dvec2 mImageMin{0.0};
            glm::dvec2 mImageSize{0.0};
    };
}

#endif