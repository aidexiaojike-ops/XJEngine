#ifndef XJ_PHYSICS_WORLD_H
#define XJ_PHYSICS_WORLD_H

#include "Physics/XJPhysicsTypes.h"

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>

namespace XJ
{
    // 一个 Play 会话对应一个物理世界。
    //
    // 所有 Jolt 类型都在 .cpp 的 Impl 里，这个头文件不出现任何 Jolt 声明。
    // 因为 Core 以 PRIVATE 链接 Jolt，Editor 和可执行程序拿不到 Jolt 的
    // include 路径，只有保持公开头干净它们才能正常编译。
    class XJPhysicsWorld
    {
        public:
            XJPhysicsWorld();
            ~XJPhysicsWorld();

            XJPhysicsWorld(const XJPhysicsWorld&) = delete;
            XJPhysicsWorld& operator=(const XJPhysicsWorld&) = delete;
            XJPhysicsWorld(XJPhysicsWorld&&) noexcept;
            XJPhysicsWorld& operator=(XJPhysicsWorld&&) noexcept;

            XJPhysicsResult Initialize(const XJPhysicsWorldConfig& config = {});
            void Shutdown();
            bool IsInitialized() const;
            // ---- 形状创建 ----
            // 无限平面。pointOnPlane 是平面上一点，normal 需为单位向量。
            // 平面负半空间是实体（Jolt 约定），所以地面用 normal=(0,1,0) +
            // point=(0,0,0) 即可。
            XJPhysicsResult CreateStaticPlane(const glm::dvec3& normal, const glm::dvec3& pointOnPlane, XJPhysicsBodyId& outBodyId);

            XJPhysicsResult CreateStaticBox(const glm::dvec3& halfExtent,
                const glm::dvec3& position, const glm::dquat& rotation,
                XJPhysicsBodyId& outBodyId);

            XJPhysicsResult CreateDynamicBox(const glm::dvec3& halfExtent,
                const glm::dvec3& position, const glm::dquat& rotation,
                double mass, XJPhysicsBodyId& outBodyId);

            XJPhysicsResult CreateDynamicSphere(double radius,
                const glm::dvec3& position, const glm::dquat& rotation,
                double mass, XJPhysicsBodyId& outBodyId);

            bool DestroyBody(XJPhysicsBodyId bodyId);

            // ---- 步进 ----
            XJPhysicsResult Step(double deltaTime);

            // ---- 状态读取 ----
            bool GetBodyPose(XJPhysicsBodyId bodyId,
                glm::dvec3& outPosition, glm::dquat& outRotation) const;
            bool GetLinearVelocity(XJPhysicsBodyId bodyId,
                glm::dvec3& outVelocity) const;
            bool IsBodyActive(XJPhysicsBodyId bodyId) const;

            // 当前存活刚体数量，供测试与统计使用。
            uint32_t GetBodyCount() const;

        private:
            class Impl;
            std::unique_ptr<Impl> mImpl;


    };
}

#endif