#ifndef XJ_EDITOR_TRANSFORM_GIZMO_VIEW_MODEL_H
#define XJ_EDITOR_TRANSFORM_GIZMO_VIEW_MODEL_H

#include "Tools/XJEditorTransformToolTypes.h"
#include "UI/XJEditorSelection.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace XJ
{
    struct XJEditorTransformGizmoViewModel//变换控件视图模型
    {
        bool Visible = false;
        XJEditorEntityId EntityId = XJ_INVALID_EDITOR_ENTITY_ID;
        XJEditorTransformSpace Space = XJEditorTransformSpace::Local;
        glm::dvec3 WorldPosition{0.0};

        // Local：物体的纯世界旋转。
        // World：单位四元数。
        glm::dquat Orientation{1.0, 0.0, 0.0, 0.0};
    };
}

#endif