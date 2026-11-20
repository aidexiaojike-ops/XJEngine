#ifndef XJ_COLLIDER_COMPONENT_H
#define XJ_COLLIDER_COMPONENT_H

#include "ECS/XJComponent.h"
#include "Physics/XJPhysicsTypes.h"

#include <glm/vec3.hpp>

#include <vector>

namespace XJ
{
    // 单个碰撞形状的配置。
    struct XJColliderShape
    {
        // 稳定标识，供 Inspector 列表增删和后续序列化使用。
        // 阶段 2 不做序列化，但字段先留着，避免以后改结构。
        uint32_t ShapeId = 0;

        XJColliderShapeType Type = XJColliderShapeType::Box;

        // 相对实体 Transform 的局部偏移与旋转（角度制）。
        glm::vec3 LocalPosition{ 0.0f };
        glm::vec3 LocalEulerDegrees{ 0.0f };

        // Box：三个轴的半长。其他类型不使用。
        glm::vec3 Size{ 0.5f };

        // Sphere / Capsule 的半径。
        float Radius = 0.5f;

        // Capsule：圆柱段的半高，不包含两端半球。
        // 总高度 = 2 * (HalfHeight + Radius)。
        // 这一点容易填错：Jolt 的 CapsuleShapeSettings 第一个参数就是这个含义。
        float HalfHeight = 0.5f;

        // 触发器标志。
        // 同一组件的所有 Shape 必须一致，混合会被物理系统拒绝。
        bool IsSensor = false;
    };

    // 碰撞体配置组件。
    //
    // 独立性很重要：Collider 不依赖 RigidBody。
    //   - RigidBody + Collider -> 按 RigidBody 配置创建
    //   - 只有 Collider        -> 自动创建 Static Body（不往 ECS 里加 RigidBody）
    //   - 只有 RigidBody       -> 不创建 Body，输出一条警告
    // 这样美术只需要给墙壁放一个 Collider，不接触质量/阻尼等刚体概念。
    class XJColliderComponent : public XJComponent
    {
        public:
            std::vector<XJColliderShape> Shapes;
            // 所有子形状的 IsSensor 是否一致。
            // 空列表返回 true（没有形状时不需要校验）。
            bool HasConsistentSensorFlag() const
            {
                if (Shapes.empty())
                    return true;

                const bool first = Shapes.front().IsSensor;
                for (const XJColliderShape& shape : Shapes)
                {
                    if (shape.IsSensor != first)
                        return false;
                }
                return true;
            }
             // 整体是否作为触发器。调用前建议先检查 HasConsistentSensorFlag。
            bool IsSensor() const
            {
                return !Shapes.empty() && Shapes.front().IsSensor;
            }
    };
}
#endif