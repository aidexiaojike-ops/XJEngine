#include "Physics/XJPhysicsSystem.h"

#include "Physics/XJPhysicsWorld.h"
#include "XJJoltConversions.h"

#include "ECS/Component/XJColliderComponent.h"
#include "ECS/Component/XJRigidBodyComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include <spdlog/spdlog.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace XJ
{
    namespace
    {
        // Jolt 的 BodyID 与引擎侧的 XJPhysicsBodyId 之间的唯一转换点。
        //
        // 两者携带的信息完全相同：BodyID 内部就是
        // "(序列号 << 23) | 索引"，GetIndexAndSequenceNumber() 取回整个 32 位值。
        // 序列号保证了刚体销毁后旧 ID 不会被新刚体复用，所以这个转换是无损的。
        XJPhysicsBodyId ToEngineBodyId(const JPH::BodyID& joltId)
        {
            XJPhysicsBodyId id;
            id.Value = joltId.GetIndexAndSequenceNumber();
            return id;
        }

        // 从实体的世界变换读出位置/旋转/缩放。
        // 没有 Transform 时退化为原点 + 单位旋转 + 单位缩放。
        struct XJEntityWorldPose
        {
            glm::dvec3 Position{ 0.0 };
            glm::dquat Rotation{ 1.0, 0.0, 0.0, 0.0 };
            glm::dvec3 Scale{ 1.0 };
        };

        XJEntityWorldPose ReadWorldPose(XJEntity& entity)
        {
            XJEntityWorldPose pose;
            if (!entity.HasComponent<XJTransformComponent>())
                return pose;

            const auto& transform = entity.GetComponent<XJTransformComponent>();
            transform.ReadWorldPosition(pose.Position);
            transform.ReadWorldRotation(pose.Rotation);
            transform.ReadLossyWorldScale(pose.Scale);
            return pose;
        }

        // 由实体上的两个组件组装出物理层的 Body 描述。
        // 返回 false 表示这个实体不参与物理（或配置非法）。
        // outError 只在"应该报错"的情况下填写。
        bool BuildBodyDesc(XJEntity& entity, XJPhysicsBodyDesc& outDesc,
            bool& outShouldWarnMissingCollider, std::string& outError)
        {
            outShouldWarnMissingCollider = false;

            const bool hasCollider = entity.HasComponent<XJColliderComponent>();
            const bool hasRigidBody = entity.HasComponent<XJRigidBodyComponent>();

            // 只有 RigidBody 没有 Collider -> 不创建，交给调用方警告一次
            if (hasRigidBody && !hasCollider)
            {
                outShouldWarnMissingCollider = true;
                return false;
            }

            // 没有 Collider 就没有形状，不参与物理
            if (!hasCollider)
                return false;

            const XJColliderComponent& collider = entity.GetComponent<XJColliderComponent>();

            if (collider.Shapes.empty())
            {
                outError = "collider has no shapes";
                return false;
            }

            // 决策 2：所有子形状的 sensor 标志必须一致
            if (!collider.HasConsistentSensorFlag())
            {
                outError = "collider mixes sensor and non-sensor shapes";
                return false;
            }

            // 默认按 Static 处理（决策 5：只有 Collider 时自动 Static）
            XJMotionType motionType = XJMotionType::Static;

            if (hasRigidBody)
            {
                const XJRigidBodyComponent& rigidBody =
                    entity.GetComponent<XJRigidBodyComponent>();

                // 决策 4：Enabled=false 不创建 Body
                if (!rigidBody.Enabled)
                    return false;

                motionType = rigidBody.MotionType;

                outDesc.Mass = double(rigidBody.Mass);
                outDesc.Friction = rigidBody.Friction;
                outDesc.Restitution = rigidBody.Restitution;
                outDesc.LinearDamping = rigidBody.LinearDamping;
                outDesc.AngularDamping = rigidBody.AngularDamping;
                outDesc.GravityFactor = rigidBody.GravityFactor;
                outDesc.AllowSleep = rigidBody.AllowSleep;
                outDesc.ContinuousCollision = rigidBody.ContinuousCollision;
                outDesc.LockLinearX = rigidBody.LockLinearX;
                outDesc.LockLinearY = rigidBody.LockLinearY;
                outDesc.LockLinearZ = rigidBody.LockLinearZ;
                outDesc.LockAngularX = rigidBody.LockAngularX;
                outDesc.LockAngularY = rigidBody.LockAngularY;
                outDesc.LockAngularZ = rigidBody.LockAngularZ;
            }

            outDesc.MotionType = motionType;
            outDesc.IsSensor = collider.IsSensor();

            // 世界姿态：用世界坐标建 Body，父子层级在写回时再处理
            const XJEntityWorldPose pose = ReadWorldPose(entity);
            outDesc.Position = pose.Position;
            outDesc.Rotation = pose.Rotation;
            outDesc.Scale = pose.Scale;

            // 形状
            outDesc.Shapes.reserve(collider.Shapes.size());
            for (const XJColliderShape& shape : collider.Shapes)
            {
                XJPhysicsShapeDesc desc;
                desc.Type = shape.Type;
                desc.LocalPosition = glm::dvec3(shape.LocalPosition);

                // 欧拉角 -> 四元数。用 Transform 组件已有的静态工具，
                // 保证和引擎其他地方的旋转约定完全一致。
                desc.LocalRotation =
                    XJTransformComponent::RotationFromEulerXYZ(
                        glm::dvec3(shape.LocalEulerDegrees));

                desc.HalfExtent = glm::dvec3(shape.Size);
                desc.Radius = double(shape.Radius);
                desc.HalfHeight = double(shape.HalfHeight);

                outDesc.Shapes.push_back(desc);
            }

            return true;
        }
    }
    class XJPhysicsSystem::Impl
    {
        public:
            Impl(XJScene& scene, XJPhysicsWorldConfig config)
                : Scene(scene),
                  SceneLifetime(scene.GetLifetimeToken()),
                  Config(config)
            {
            }

            ~Impl() { Destroy(); }

            void Create();
            void FixedUpdate(float fixedDeltaTime);
            void Destroy();

            void MarkDirty(entt::entity entity);
            bool QueueTeleport(entt::entity entity,
                const glm::dvec3& position, const glm::dquat& rotation);
            XJPhysicsSystem::Statistics BuildStatistics() const;

        private:
            bool HasValidScene() const
            {
                return !SceneLifetime.expired();
            }

            // 结构同步：在 StructureRevision 变化时做全表 diff
            void SyncStructure();

            // 尝试为实体创建 Body。失败时记录日志并返回 false。
            bool TryCreateBody(XJEntity& entity);

            // 销毁某个实体的 Body（如果存在），并从双向表移除
            void DestroyBodyOf(entt::entity entity);

            // 销毁所有 Body（OnDestroy 和结构重建时用）
            void DestroyAllBodies();

            // BeforeEntityDestroy 回调
            void OnBeforeEntityDestroy(XJEntity& entity) noexcept;

            // 写回 Dynamic Body 的位置与旋转到 ECS Transform
            void WriteBackTransforms();

            // 检测 Dynamic 实体的缩放变化（只警告，不重建 Shape）
            void DetectScaleChanges();

            XJScene& Scene;
            std::weak_ptr<void> SceneLifetime;
            XJPhysicsWorldConfig Config;

            XJPhysicsWorld World;
            bool WorldReady = false;

            // ---- BodyID 双向映射（只在这个 Impl 内）----
            std::unordered_map<entt::entity, JPH::BodyID> EntityToBody;
            std::unordered_map<JPH::BodyID, entt::entity> BodyToEntity;

            // 创建时记录的世界缩放，用于检测运行期缩放变化
            std::unordered_map<entt::entity, glm::dvec3> EntityCreatedScale;

            // ---- 三层同步状态 ----
            entt::connection DestroyConnection;
            uint64_t LastStructureRevision = 0;
            std::unordered_set<entt::entity> PendingDestroy;
            std::unordered_set<entt::entity> DirtyEntities;

            // ---- 命令队列 ----
            struct TeleportCommand
            {
                entt::entity Entity = entt::null;
                glm::dvec3 Position{ 0.0 };
                glm::dquat Rotation{ 1.0, 0.0, 0.0, 0.0 };
            };
            std::vector<TeleportCommand> PendingTeleports;

            // ---- 一次性警告去重 ----
            std::unordered_set<entt::entity> WarnedWriteBackFailure;
            std::unordered_set<entt::entity> WarnedScaleChange;
            std::unordered_set<entt::entity> WarnedMissingCollider;

            uint64_t TotalSteps = 0;
            uint64_t FailedWriteBacks = 0;
    };
    void XJPhysicsSystem::Impl::Create()
    {
        if (!HasValidScene())
        {
            spdlog::error("XJPhysicsSystem::Create failed: scene is no longer alive.");
            return;
        }

        if (World.Initialize(Config) != XJPhysicsResult::Success)
        {
            spdlog::error("XJPhysicsSystem::Create failed: physics world initialization failed.");
            return;
        }
        WorldReady = true;

        // 连接实体销毁通知。照抄 XJScriptSystem 的用法。
        DestroyConnection = Scene.BeforeEntityDestroy()
            .connect<&Impl::OnBeforeEntityDestroy>(*this);

        // 记录当前结构版本，并做一次全量扫描
        LastStructureRevision = Scene.GetStructureRevision();
        SyncStructure();

        spdlog::info("XJPhysicsSystem created: bodies={}", EntityToBody.size());
    }

    void XJPhysicsSystem::Impl::Destroy()
    {
        // 释放顺序：先断开通知，再销毁 Body，最后销毁 World。
        if (DestroyConnection)
            DestroyConnection.release();

        if (WorldReady)
        {
            DestroyAllBodies();
            World.Shutdown();
            WorldReady = false;
        }

        PendingDestroy.clear();
        DirtyEntities.clear();
        PendingTeleports.clear();
        WarnedWriteBackFailure.clear();
        WarnedScaleChange.clear();
        WarnedMissingCollider.clear();
        LastStructureRevision = 0;
        TotalSteps = 0;
        FailedWriteBacks = 0;
    }

    void XJPhysicsSystem::Impl::OnBeforeEntityDestroy(XJEntity& entity) noexcept
    {
        // 只入队，不做任何实际删除。
        // 真正的 DestroyBody 在下一个 FixedUpdate 开头执行，
        // 因为回调发生时物理世界可能正在被遍历。
        PendingDestroy.insert(entity.GetEcsEntity());
    }

    void XJPhysicsSystem::Impl::SyncStructure()
    {
        if (!HasValidScene() || !WorldReady)
            return;

        const entt::registry& registry = Scene.XJGetEcsRegistry();

        // ---- 1. 找出所有应该存在 Body 的实体 ----
        std::unordered_set<entt::entity> alive;
        auto view = registry.view<XJColliderComponent>();
        for (entt::entity ecsEntity : view)
        {
            // 已排队销毁的实体不再参与物理，即使它还在 registry 里
            if (Scene.IsPendingDestroy(ecsEntity))
                continue;

            XJEntity* entity = Scene.GetEntity(ecsEntity);
            if (!entity || !entity->IsValid())
                continue;

            alive.insert(ecsEntity);

            if (!EntityToBody.contains(ecsEntity))
                TryCreateBody(*entity);
        }

        // ---- 2. 找出需要销毁的 ----
        std::vector<entt::entity> orphans;
        orphans.reserve(EntityToBody.size());
        for (const auto& pair : EntityToBody)
        {
            if (!alive.contains(pair.first))
                orphans.push_back(pair.first);
        }

        for (entt::entity ecsEntity : orphans)
            DestroyBodyOf(ecsEntity);
    }
    bool XJPhysicsSystem::Impl::TryCreateBody(XJEntity& entity)
    {
        XJPhysicsBodyDesc desc;
        bool shouldWarnMissingCollider = false;
        std::string error;

        if (!BuildBodyDesc(entity, desc, shouldWarnMissingCollider, error))
        {
            if (shouldWarnMissingCollider)
            {
                const entt::entity ecsEntity = entity.GetEcsEntity();
                if (WarnedMissingCollider.insert(ecsEntity).second)
                {
                    spdlog::warn(
                        "Entity has XJRigidBodyComponent but no XJColliderComponent; "
                        "no physics body will be created. entity={}",
                        static_cast<uint32_t>(ecsEntity));
                }
            }
            else if (!error.empty())
            {
                spdlog::error("Physics body creation rejected for entity {}: {}",
                    static_cast<uint32_t>(entity.GetEcsEntity()), error);
            }
            return false;
        }

        XJPhysicsBodyId bodyId;
        if (World.CreateBody(desc, bodyId) != XJPhysicsResult::Success)
        {
            // World 内部已经记录了具体原因
            return false;
        }

        const JPH::BodyID joltId(bodyId.Value);
        const entt::entity ecsEntity = entity.GetEcsEntity();

        EntityToBody.emplace(ecsEntity, joltId);
        BodyToEntity.emplace(joltId, ecsEntity);
        EntityCreatedScale.emplace(ecsEntity, desc.Scale);

        // 新创建的实体允许重新警告
        WarnedWriteBackFailure.erase(ecsEntity);
        WarnedScaleChange.erase(ecsEntity);
        WarnedMissingCollider.erase(ecsEntity);

        return true;
    }

    void XJPhysicsSystem::Impl::DestroyBodyOf(entt::entity ecsEntity)
    {
        const auto it = EntityToBody.find(ecsEntity);
        if (it != EntityToBody.end())
        {
            const JPH::BodyID joltId = it->second;

            World.DestroyBody(ToEngineBodyId(joltId));

            BodyToEntity.erase(joltId);
            EntityToBody.erase(it);
        }

        EntityCreatedScale.erase(ecsEntity);
        WarnedWriteBackFailure.erase(ecsEntity);
        WarnedScaleChange.erase(ecsEntity);
        WarnedMissingCollider.erase(ecsEntity);
    }

    void XJPhysicsSystem::Impl::DestroyAllBodies()
    {
        // 先收集再销毁，避免在遍历 map 时修改它
        std::vector<entt::entity> entities;
        entities.reserve(EntityToBody.size());
        for (const auto& pair : EntityToBody)
            entities.push_back(pair.first);

        for (entt::entity ecsEntity : entities)
            DestroyBodyOf(ecsEntity);

        EntityToBody.clear();
        BodyToEntity.clear();
        EntityCreatedScale.clear();
    }
    void XJPhysicsSystem::Impl::FixedUpdate(float fixedDeltaTime)
    {
        if (!HasValidScene() || !WorldReady)
            return;

        // ---- 1. 处理待销毁 ----
        if (!PendingDestroy.empty())
        {
            std::vector<entt::entity> toDestroy(PendingDestroy.begin(), PendingDestroy.end());
            PendingDestroy.clear();
            for (entt::entity ecsEntity : toDestroy)
                DestroyBodyOf(ecsEntity);
        }

        // ---- 2. 结构同步（只在结构版本变化时真正执行）----
        const uint64_t revision = Scene.GetStructureRevision();
        if (revision != LastStructureRevision)
        {
            LastStructureRevision = revision;
            SyncStructure();
        }

        // ---- 3. 处理脏标记重建 ----
        if (!DirtyEntities.empty())
        {
            std::vector<entt::entity> dirty(DirtyEntities.begin(), DirtyEntities.end());
            DirtyEntities.clear();

            for (entt::entity ecsEntity : dirty)
            {
                DestroyBodyOf(ecsEntity);

                XJEntity* entity = Scene.GetEntity(ecsEntity);
                if (entity && entity->IsValid() && !Scene.IsPendingDestroy(ecsEntity))
                    TryCreateBody(*entity);
            }
        }

        // ---- 4. 处理传送请求 ----
        if (!PendingTeleports.empty())
        {
            std::vector<TeleportCommand> teleports = std::move(PendingTeleports);
            PendingTeleports.clear();

            for (const TeleportCommand& command : teleports)
            {
                const auto it = EntityToBody.find(command.Entity);
                if (it == EntityToBody.end())
                {
                    spdlog::warn(
                        "Teleport request ignored: entity {} has no physics body.",
                        static_cast<uint32_t>(command.Entity));
                    continue;
                }

                if (!World.SetBodyPose(ToEngineBodyId(it->second),
                        command.Position, command.Rotation, true))
                {
                    spdlog::warn(
                        "Teleport request failed for entity {}.",
                        static_cast<uint32_t>(command.Entity));
                }
            }
        }

        // ---- 5. 步进 ----
        const XJPhysicsResult stepResult = World.Step(double(fixedDeltaTime));
        if (stepResult != XJPhysicsResult::Success)
        {
            spdlog::error("Physics step failed, result={}",
                XJPhysicsResultToString(stepResult));
            return;
        }
        ++TotalSteps;

        // ---- 6. 写回 Dynamic 的位姿 ----
        WriteBackTransforms();

        // ---- 7. 缩放变化检测（只警告，不重建形状）----
        DetectScaleChanges();
    }
    void XJPhysicsSystem::Impl::WriteBackTransforms()
    {
        if (!HasValidScene())
            return;

        for (const auto& pair : EntityToBody)
        {
            const entt::entity ecsEntity = pair.first;

            // 已排队销毁的实体不写回，避免把即将消失的实体的 Transform 改动
            if (Scene.IsPendingDestroy(ecsEntity))
                continue;

            XJEntity* entity = Scene.GetEntity(ecsEntity);
            if (!entity || !entity->IsValid())
                continue;

            // 只有 Dynamic 由物理系统拥有位置和旋转写入权。
            // Kinematic 由游戏逻辑驱动，Static 根本不该动。
            if (entity->HasComponent<XJRigidBodyComponent>())
            {
                const XJRigidBodyComponent& rigidBody =
                    entity->GetComponent<XJRigidBodyComponent>();
                if (rigidBody.MotionType != XJMotionType::Dynamic)
                    continue;
            }
            else
            {
                // 只有 Collider 的是隐式 Static，不写回
                continue;
            }

            if (!entity->HasComponent<XJTransformComponent>())
                continue;

            glm::dvec3 position{ 0.0 };
            glm::dquat rotation{ 1.0, 0.0, 0.0, 0.0 };
            if (!World.GetBodyPose(ToEngineBodyId(pair.second), position, rotation))
                continue;

            XJTransformComponent& transform =
                entity->GetComponent<XJTransformComponent>();

            const XJTransformStatus status = transform.TrySetWorldPose(position, rotation);
            if (status == XJTransformStatus::Success)
                continue;

            ++FailedWriteBacks;

            // 首次失败才警告，避免每帧刷屏
            if (WarnedWriteBackFailure.insert(ecsEntity).second)
            {
                spdlog::warn(
                    "Physics write-back failed for entity {} (status={}). "
                    "The parent hierarchy likely cannot represent the body pose. "
                    "This entity will be skipped until it is recreated.",
                    static_cast<uint32_t>(ecsEntity), static_cast<int>(status));
            }
        }
    }
    void XJPhysicsSystem::Impl::DetectScaleChanges()
    {
        if (!HasValidScene())
            return;

        for (const auto& pair : EntityToBody)
        {
            const auto scaleIt = EntityCreatedScale.find(pair.first);
            if (scaleIt == EntityCreatedScale.end())
                continue;

            XJEntity* entity = Scene.GetEntity(pair.first);
            if (!entity || !entity->IsValid())
                continue;
            if (!entity->HasComponent<XJTransformComponent>())
                continue;

            glm::dvec3 currentScale{ 1.0 };
            entity->GetComponent<XJTransformComponent>()
                .ReadLossyWorldScale(currentScale);

            const glm::dvec3 delta = currentScale - scaleIt->second;
            constexpr double kEpsilon = 1.0e-4;
            if (std::abs(delta.x) < kEpsilon && std::abs(delta.y) < kEpsilon &&
                std::abs(delta.z) < kEpsilon)
                continue;

            // 缩放不重建形状：重建会导致接触状态丢失、刚体突然弹开，
            // 而且开销很大。阶段 2 明确不支持运行期改缩放。
            if (WarnedScaleChange.insert(pair.first).second)
            {
                spdlog::warn(
                    "Entity {} changed world scale after body creation. "
                    "Physics shape keeps its original size. "
                    "Recreate the body (MarkPhysicsDirty) if this is intended.",
                    static_cast<uint32_t>(pair.first));
            }
        }
    }
    void XJPhysicsSystem::Impl::MarkDirty(entt::entity entity)
    {
        if (entity == entt::null)
            return;
        DirtyEntities.insert(entity);
    }

    bool XJPhysicsSystem::Impl::QueueTeleport(entt::entity entity,
        const glm::dvec3& position, const glm::dquat& rotation)
    {
        if (entity == entt::null || !std::isfinite(position.x) ||
            !std::isfinite(position.y) || !std::isfinite(position.z))
            return false;

        TeleportCommand command;
        command.Entity = entity;
        command.Position = position;
        command.Rotation = rotation;
        PendingTeleports.push_back(command);
        return true;
    }

    XJPhysicsSystem::Statistics XJPhysicsSystem::Impl::BuildStatistics() const
    {
        XJPhysicsSystem::Statistics stats;
        stats.BodyCount = static_cast<uint32_t>(EntityToBody.size());
        stats.PendingTeleports = static_cast<uint32_t>(PendingTeleports.size());
        stats.DirtyEntities = static_cast<uint32_t>(DirtyEntities.size());
        stats.TotalSteps = TotalSteps;
        stats.FailedWriteBacks = FailedWriteBacks;
        return stats;
    }

    // ---------------- 对外转发 ----------------

    XJPhysicsSystem::XJPhysicsSystem(XJScene& scene, XJPhysicsWorldConfig config)
        : mImpl(std::make_unique<Impl>(scene, config))
    {
    }

    XJPhysicsSystem::~XJPhysicsSystem()
    {
        // Impl 的析构会调 Destroy()，这里不用重复
    }

    void XJPhysicsSystem::OnCreate()
    {
        mImpl->Create();
    }

    void XJPhysicsSystem::OnFixedUpdate(float fixedDeltaTime)
    {
        mImpl->FixedUpdate(fixedDeltaTime);
    }

    void XJPhysicsSystem::OnDestroy()
    {
        mImpl->Destroy();
    }

    void XJPhysicsSystem::MarkPhysicsDirty(XJEntity& entity)
    {
        if (!entity.IsValid())
            return;
        mImpl->MarkDirty(entity.GetEcsEntity());
    }

    bool XJPhysicsSystem::RequestTeleport(XJEntity& entity,
        const glm::dvec3& position, const glm::dquat& rotation)
    {
        if (!entity.IsValid())
            return false;
        return mImpl->QueueTeleport(entity.GetEcsEntity(), position, rotation);
    }

    XJPhysicsSystem::Statistics XJPhysicsSystem::GetStatistics() const
    {
        return mImpl ? mImpl->BuildStatistics() : Statistics{};
    }
}