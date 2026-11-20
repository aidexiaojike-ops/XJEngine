#ifndef XJ_RIGID_BODY_COMPONENT_H
#define XJ_RIGID_BODY_COMPONENT_H

#include "ECS/XJComponent.h"
#include "Physics/XJPhysicsTypes.h"

namespace XJ
{
    // 刚体配置组件。
    //
    // 重要：这个组件只保存"配置"，不保存任何运行时句柄。
    // Jolt 的 BodyID、Shape、PhysicsSystem 全部活在
    // XJPhysicsSystem::Impl 内部，不会进入 ECS 组件、场景文件或脚本。
    //
    // 字段是 public 的，Inspector 可以直接读写。
    // 但运行期修改字段后必须调用 XJPhysicsSystem::MarkPhysicsDirty(entity)，
    // 否则物理系统不知道配置变了（EnTT 无法检测到字段级修改）。
    class XJRigidBodyComponent : public XJComponent
    {
        public:
            // Static：不移动，不响应力
            // Kinematic：由游戏逻辑/脚本驱动，不响应力
            // Dynamic：完全由物理模拟驱动
            XJMotionType MotionType = XJMotionType::Dynamic;

            // 仅 Dynamic 使用，必须 > 0。
            // 惯性张量由形状密度按质量推算，不需要另填。
            float Mass = 1.0f;

            float LinearDamping = 0.05f;
            float AngularDamping = 0.05f;

            // 0 = 不受重力，1 = 正常重力，也可 >1 或 <0
            float GravityFactor = 1.0f;
            bool AllowSleep = true;

            // true -> Jolt 的 EMotionQuality::LinearCast（连续碰撞检测，防穿模）
            bool ContinuousCollision = false;

            // ---- Body 级物理材质 ----
            // 阶段 2 放在这里而不是 Collider，因为 Jolt 的摩擦/弹性
            // 是 Body 级属性：默认的 CombineFunction 直接忽略 SubShapeID。
            // 把这两个字段放到子形状上会造成"填了但不生效"的陷阱。
            float Friction = 0.2f;
            float Restitution = 0.0f;

            // ---- 轴向锁 ----
            // 拆成 6 个具名 bool 而不是 bool[3][2]：
            // Inspector 可以直接画复选框，不需要拼标签字符串。
            // 注意：六个全锁会被物理系统拒绝——
            // Jolt 的 EAllowedDOFs::None 是非法值，会导致崩溃。
            bool LockLinearX = false;
            bool LockLinearY = false;
            bool LockLinearZ = false;
            bool LockAngularX = false;
            bool LockAngularY = false;
            bool LockAngularZ = false;

            // false = 不创建物理 Body（不占资源）。
            // 运行期切换需调用 MarkPhysicsDirty。
            bool Enabled = true;
            
    };
}

#endif