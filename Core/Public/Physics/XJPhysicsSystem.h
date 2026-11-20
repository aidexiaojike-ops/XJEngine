#ifndef XJ_PHYSICS_SYSTEM_H
#define XJ_PHYSICS_SYSTEM_H

#include "ECS/XJSystem.h"
#include "Physics/XJPhysicsTypes.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>

namespace XJ
{
    class XJScene;
    class XJEntity;

    // ECS 与 Jolt 之间的桥。
    //
    // 生命周期与 Play 会话一致：
    //   OnCreate      -> 初始化 XJPhysicsWorld，全量扫描实体建 Body
    //   OnFixedUpdate -> 同步结构 / 处理命令 / Step / 写回 Transform
    //   OnDestroy     -> 销毁所有 Body，再释放 World
    //
    // 结构照抄 XJScriptSystem：公开头不出现任何 Jolt 类型，
    // Jolt 的一切都在 .cpp 的 Impl 里。
    class XJPhysicsSystem final : public XJSystem
    {
        public:
            explicit XJPhysicsSystem(XJScene& scene, XJPhysicsWorldConfig config = {});
            ~XJPhysicsSystem() override;

            XJPhysicsSystem(const XJPhysicsSystem&) = delete;
            XJPhysicsSystem& operator=(const XJPhysicsSystem&) = delete;

            void OnCreate() override;
            void OnFixedUpdate(float fixedDeltaTime) override;
            void OnDestroy() override;

            // 组件配置变了（改 MotionType/Mass/Shapes/Enabled 等），
            // 通知物理系统在下一个 fixed step 重建对应 Body。
            // 直接改组件字段是无法被 EnTT 检测到的，必须显式调用。
            void MarkPhysicsDirty(XJEntity& entity);

            // 传送请求：入队，在下一个 fixed step 的 Step 之前执行。
            // 不能在脚本执行中直接改正在模拟的物理世界。
            bool RequestTeleport(XJEntity& entity,
                const glm::dvec3& position, const glm::dquat& rotation);

            struct Statistics
            {
                uint32_t BodyCount = 0;
                uint32_t PendingTeleports = 0;
                uint32_t DirtyEntities = 0;
                uint64_t TotalSteps = 0;
                uint64_t FailedWriteBacks = 0;
            };

            Statistics GetStatistics() const;

        private:
            class Impl;
            std::unique_ptr<Impl> mImpl;
    };
}

#endif