#ifndef XJ_TRANSFORM_GIZMO_INTERACTION_MATH_H
#define XJ_TRANSFORM_GIZMO_INTERACTION_MATH_H

#include "Tools/XJTransformGizmoInteractionTypes.h"

namespace XJ
{
    bool BuildGizmoRay(const glm::dmat4& inverseViewProjection, const glm::dvec2& uv, XJGizmoRay& ray);//构建Gizmo射线
    XJTransformGizmoHandle PickTransformGizmo(const XJGizmoRenderedFrame& frame, const glm::dvec2& uv, const glm::dvec2& imageSize);//拾取变换控件
    bool BeginGizmoMoveConstraint(const XJGizmoRenderedFrame& frame, XJTransformGizmoHandle handle, const glm::dvec2& uv, XJGizmoMoveConstraint& constraint);//开始Gizmo移动约束

    bool CalculateGizmoMoveTarget(const XJGizmoMoveConstraint& constraint, const glm::dvec2& uv, glm::dvec3& target);//计算Gizmo移动目标

    glm::dvec3 GetGizmoUnitAxis(XJTransformGizmoHandle handle);//获取单位轴

    bool BeginGizmoRotateConstraint(const XJGizmoRenderedFrame& frame, XJTransformGizmoHandle handle, const glm::dvec2& uv, XJGizmoRotateConstraint& constraint);//开始旋转约束工具
    bool SampleGizmoRotateConstraint(XJGizmoRotateConstraint& constraint, const glm::dvec2& uv);//示例工具旋转
    bool CalculateGizmoScaleFactor(const XJGizmoMoveConstraint& constraint, const glm::dvec2& uv, double referenceLength, double& factor);//计算工具比例因子
}
#endif