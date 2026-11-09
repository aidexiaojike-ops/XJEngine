#ifndef XJ_TRANSFORM_GIZMO_INTERACTION_TYPES_H
#define XJ_TRANSFORM_GIZMO_INTERACTION_TYPES_H

#include "Tools/XJTransformGizmoGeometry.h"
#include "Tools/XJEditorTransformToolTypes.h"
#include "Tools/XJEditorTransformEditTypes.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>

namespace XJ
{
    // 材质系统发布的绘制快照。
    struct XJGizmoRenderedFrame
    {
        bool Valid = false;

        uint64_t Epoch = 0;// 当前帧的唯一标识符，用于区分不同的绘制快照。
        uint64_t EntityId = 0;// 当前帧对应的实体 ID，用于关联 Gizmo 与场景中的实体。
        uint64_t SettingsRevision = 0;// 当前帧的 Gizmo 设置版本号，用于判断 Gizmo 设置是否发生变化。

        XJEditorTransformSpace Space = XJEditorTransformSpace::Local;//本地空间

        uint32_t Width = 0;
        uint32_t Height = 0;

        glm::mat4 View{1.0f};// 视图矩阵，默认初始化为单位矩阵
        glm::mat4 Projection{1.0f};
        glm::mat4 Model{1.0f};
        
        glm::dvec3 WorldPosition{0.0};
        glm::dquat Orientation{1.0, 0.0, 0.0, 0.0};
    };

    // View 提交的输入快照，不含 ECS 指针。
    struct XJGizmoPointerInput
    {
        bool Pending = false;// 是否有待处理的输入事件
        
        bool Inside = false;// 鼠标是否在 Gizmo 内部
        bool Pressed = false;// 鼠标是否按下
        bool Down = false;// 鼠标是否按下并保持
        bool Released = false;// 鼠标是否释放 
        bool Cancel  = true;// 鼠标是否取消

        // 相对于图像区域的 UV。
        // 拖出图像后允许小于 0 或大于 1。
        glm::dvec2 UV{0.0};

        glm::dvec2 ImageMin{0.0};
        glm::dvec2 ImageSize{0.0};

        XJGizmoRenderedFrame Frame;
    };

    struct XJGizmoInteractionViewModel
    {
        XJTransformGizmoHandle Hovered = XJTransformGizmoHandle::None;// 当前悬停的 Gizmo 句柄
        XJTransformGizmoHandle Active = XJTransformGizmoHandle::None;// 当前激活的 Gizmo 句柄

        bool Dragging = false;// 是否正在拖拽 Gizmo
        std::string Error;
        
    };

    struct XJGizmoRay// 射线
    {
        glm::dvec3 Origin{0.0};
        glm::dvec3 Direction{0.0, 0.0, -1.0};

        double MaximumDistance = 0.0;
    };

    // 按下时冻结的移动约束。
    struct XJGizmoMoveConstraint
    {
        glm::dmat4 InverseViewProjection{1.0};

        glm::dvec3 StartPosition{0.0};
        glm::dvec3 Axis{1.0, 0.0, 0.0};
        glm::dvec3 PlaneNormal{0.0, 0.0, 1.0};

        double StartParameter = 0.0;
    };

    struct XJGizmoTransformSnapshot//坐标轴快照
    {
        uint64_t EntityId = 0;
        uint64_t StructureRevision = 0;
        uint64_t ParentId = 0;

        glm::vec3 LocalPosition{0.0f};
        glm::quat LocalRotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 LocalScale{1.0f};

        glm::dvec3 WorldPosition{0.0};
        glm::dquat WorldRotation{1.0, 0.0, 0.0, 0.0};
        glm::dvec3 WorldScale{1.0};

        // 用于检测拖动期间父级姿态是否发生变化。
        glm::dmat4 ParentMatrix{1.0};
        glm::dquat ParentRotation{1.0, 0.0, 0.0, 0.0};
    };

    // Controller 计算出的绝对目标，不是逐帧增量。
    struct XJGizmoPreviewValue
    {
        XJTransformGizmoOperation Operation = XJTransformGizmoOperation::None;
        XJEditorTransformSpace Space = XJEditorTransformSpace::Local;

        glm::dvec3 Position{0.0};
        glm::dquat Rotation{1.0,0.0,0.0,0.0};
        glm::dvec3 Scale{1.0};
    };

    struct XJGizmoRotateConstraint
    {
        glm::dmat4 InverseViewProjection{1.0};
        glm::dvec3 Center{0.0};
        glm::dvec3 Axis{1.0,0.0,0.0};

        glm::dvec3 PreviousDirection{0.0, 1.0, 0.0};

        double MinimumRadius = 0.0;
        double TotalAngle = 0.0;
    };
}

#endif
