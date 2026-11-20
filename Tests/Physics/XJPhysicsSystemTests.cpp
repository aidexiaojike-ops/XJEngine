#include "Physics/XJPhysicsSystem.h"

#include "ECS/Component/XJColliderComponent.h"
#include "ECS/Component/XJRigidBodyComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#include <cmath>
#include <cstdlib>
#include <string_view>

namespace
{
    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    constexpr float kFixedDeltaTime = 1.0f / 60.0f;

    // 给实体加一个 Box 碰撞体
    void AddBoxCollider(XJ::XJEntity& entity, const glm::vec3& halfExtent,
        bool isSensor = false)
    {
        XJ::XJColliderShape shape;
        shape.Type = XJ::XJColliderShapeType::Box;
        shape.Size = halfExtent;
        shape.IsSensor = isSensor;

        auto& collider = entity.AddComponent<XJ::XJColliderComponent>();
        collider.Shapes.push_back(shape);
    }

    // 静态地面 + 动态箱子，跑 120 步看是否落地
    void TestDynamicBoxFallsOnColliderOnlyGround()
    {
        XJ::XJScene scene;
        XJ::XJPhysicsSystem physics(scene);

        physics.OnCreate();
        Check(physics.GetStatistics().BodyCount == 0, "no bodies before entities");

        // 地面：只有 Collider，应该自动变成 Static
        XJ::XJEntity* ground = scene.CreateEntityWithTransform("Ground");
        Check(ground != nullptr, "ground created");
        AddBoxCollider(*ground, glm::vec3(20.0f, 0.5f, 20.0f));
        ground->GetComponent<XJ::XJTransformComponent>()
            .SetLocalPosition(glm::dvec3(0.0, -0.5, 0.0));

        // 箱子：RigidBody + Collider，Dynamic
        XJ::XJEntity* box = scene.CreateEntityWithTransform("Box");
        Check(box != nullptr, "box created");
        AddBoxCollider(*box, glm::vec3(0.5f));
        auto& rigidBody = box->AddComponent<XJ::XJRigidBodyComponent>();
        rigidBody.MotionType = XJ::XJMotionType::Dynamic;
        rigidBody.Mass = 1.0f;
        box->GetComponent<XJ::XJTransformComponent>()
            .SetLocalPosition(glm::dvec3(0.0, 5.0, 0.0));

        // 让物理系统看到这两个实体
        physics.MarkPhysicsDirty(*ground);
        physics.MarkPhysicsDirty(*box);

        for (int i = 0; i < 120; ++i)
            physics.OnFixedUpdate(kFixedDeltaTime);

        Check(physics.GetStatistics().BodyCount == 2, "two bodies created");

        glm::dvec3 position{};
        Check(box->GetComponent<XJ::XJTransformComponent>()
                      .ReadWorldPosition(position) == XJ::XJTransformStatus::Success,
            "box world position readable");

        Check(std::abs(position.y - 0.5) < 0.1, "box rests on ground");

        physics.OnDestroy();
        Check(physics.GetStatistics().BodyCount == 0, "bodies destroyed");
    }

    // Enabled=false 不创建 Body；改回 true + MarkDirty 后创建
    void TestDisabledRigidBodyCreatesNoBody()
    {
        XJ::XJScene scene;
        XJ::XJPhysicsSystem physics(scene);
        physics.OnCreate();

        XJ::XJEntity* entity = scene.CreateEntityWithTransform("Disabled");
        AddBoxCollider(*entity, glm::vec3(0.5f));
        auto& rigidBody = entity->AddComponent<XJ::XJRigidBodyComponent>();
        rigidBody.MotionType = XJ::XJMotionType::Dynamic;
        rigidBody.Enabled = false;

        physics.MarkPhysicsDirty(*entity);
        physics.OnFixedUpdate(kFixedDeltaTime);
        Check(physics.GetStatistics().BodyCount == 0, "disabled body not created");

        rigidBody.Enabled = true;
        physics.MarkPhysicsDirty(*entity);
        physics.OnFixedUpdate(kFixedDeltaTime);
        Check(physics.GetStatistics().BodyCount == 1, "enabled body created");

        physics.OnDestroy();
    }

    // RigidBody 没有 Collider -> 不创建，只警告
    void TestRigidBodyWithoutColliderIsIgnored()
    {
        XJ::XJScene scene;
        XJ::XJPhysicsSystem physics(scene);
        physics.OnCreate();

        XJ::XJEntity* entity = scene.CreateEntityWithTransform("NoCollider");
        auto& rigidBody = entity->AddComponent<XJ::XJRigidBodyComponent>();
        rigidBody.MotionType = XJ::XJMotionType::Dynamic;

        physics.MarkPhysicsDirty(*entity);
        physics.OnFixedUpdate(kFixedDeltaTime);

        Check(physics.GetStatistics().BodyCount == 0, "body without collider ignored");

        physics.OnDestroy();
    }

    // 混合 sensor 标志 -> 拒绝创建
    void TestMixedSensorFlagsRejected()
    {
        XJ::XJScene scene;
        XJ::XJPhysicsSystem physics(scene);
        physics.OnCreate();

        XJ::XJEntity* entity = scene.CreateEntityWithTransform("Mixed");
        auto& collider = entity->AddComponent<XJ::XJColliderComponent>();

        XJ::XJColliderShape normal;
        normal.Type = XJ::XJColliderShapeType::Box;
        normal.Size = glm::vec3(0.5f);
        normal.IsSensor = false;
        collider.Shapes.push_back(normal);

        XJ::XJColliderShape sensor = normal;
        sensor.IsSensor = true;
        collider.Shapes.push_back(sensor);

        physics.MarkPhysicsDirty(*entity);
        physics.OnFixedUpdate(kFixedDeltaTime);

        Check(physics.GetStatistics().BodyCount == 0, "mixed sensor flags rejected");

        physics.OnDestroy();
    }

    // 六轴全锁 -> 拒绝创建
    void TestAllAxesLockedRejected()
    {
        XJ::XJScene scene;
        XJ::XJPhysicsSystem physics(scene);
        physics.OnCreate();

        XJ::XJEntity* entity = scene.CreateEntityWithTransform("Locked");
        AddBoxCollider(*entity, glm::vec3(0.5f));

        auto& rigidBody = entity->AddComponent<XJ::XJRigidBodyComponent>();
        rigidBody.MotionType = XJ::XJMotionType::Dynamic;
        rigidBody.LockLinearX = true;
        rigidBody.LockLinearY = true;
        rigidBody.LockLinearZ = true;
        rigidBody.LockAngularX = true;
        rigidBody.LockAngularY = true;
        rigidBody.LockAngularZ = true;

        physics.MarkPhysicsDirty(*entity);
        physics.OnFixedUpdate(kFixedDeltaTime);

        Check(physics.GetStatistics().BodyCount == 0, "all axes locked rejected");

        physics.OnDestroy();
    }

    // 传送请求在下一个 fixed step 生效
    void TestTeleportApplied()
    {
        XJ::XJScene scene;
        XJ::XJPhysicsSystem physics(scene);
        physics.OnCreate();

        XJ::XJEntity* entity = scene.CreateEntityWithTransform("Teleporting");
        AddBoxCollider(*entity, glm::vec3(0.5f));
        auto& rigidBody = entity->AddComponent<XJ::XJRigidBodyComponent>();
        rigidBody.MotionType = XJ::XJMotionType::Dynamic;
        rigidBody.GravityFactor = 0.0f;   // 关掉重力，位置才稳定

        physics.MarkPhysicsDirty(*entity);
        physics.OnFixedUpdate(kFixedDeltaTime);

        Check(physics.RequestTeleport(*entity, glm::dvec3(3.0, 7.0, 0.0),
                  glm::dquat(1.0, 0.0, 0.0, 0.0)),
            "teleport queued");

        physics.OnFixedUpdate(kFixedDeltaTime);

        glm::dvec3 position{};
        entity->GetComponent<XJ::XJTransformComponent>().ReadWorldPosition(position);
        Check(std::abs(position.y - 7.0) < 0.1, "teleport applied");

        physics.OnDestroy();
    }

    // 实体销毁后 Body 必须被移除
    void TestEntityDestroyRemovesBody()
    {
        XJ::XJScene scene;
        XJ::XJPhysicsSystem physics(scene);
        physics.OnCreate();

        XJ::XJEntity* entity = scene.CreateEntityWithTransform("Doomed");
        AddBoxCollider(*entity, glm::vec3(0.5f));
        auto& rigidBody = entity->AddComponent<XJ::XJRigidBodyComponent>();
        rigidBody.MotionType = XJ::XJMotionType::Dynamic;

        physics.MarkPhysicsDirty(*entity);
        physics.OnFixedUpdate(kFixedDeltaTime);
        Check(physics.GetStatistics().BodyCount == 1, "body created");

        scene.DestroyEntity(entity);
        physics.OnFixedUpdate(kFixedDeltaTime);

        Check(physics.GetStatistics().BodyCount == 0, "body removed after destroy");

        physics.OnDestroy();
    }

    // 重复 Play/Stop 不应泄漏
    void TestRepeatedCreateDestroy()
    {
        for (int round = 0; round < 5; ++round)
        {
            XJ::XJScene scene;
            XJ::XJPhysicsSystem physics(scene);
            physics.OnCreate();

            XJ::XJEntity* entity = scene.CreateEntityWithTransform("Box");
            AddBoxCollider(*entity, glm::vec3(0.5f));
            auto& rigidBody = entity->AddComponent<XJ::XJRigidBodyComponent>();
            rigidBody.MotionType = XJ::XJMotionType::Dynamic;

            physics.MarkPhysicsDirty(*entity);
            for (int i = 0; i < 10; ++i)
                physics.OnFixedUpdate(kFixedDeltaTime);

            Check(physics.GetStatistics().BodyCount == 1, "body exists on every round");
            physics.OnDestroy();
        }
    }
}

int main()
{
    TestDynamicBoxFallsOnColliderOnlyGround();
    TestDisabledRigidBodyCreatesNoBody();
    TestRigidBodyWithoutColliderIsIgnored();
    TestMixedSensorFlagsRejected();
    TestAllAxesLockedRejected();
    TestTeleportApplied();
    TestEntityDestroyRemovesBody();
    TestRepeatedCreateDestroy();

    spdlog::info("XJPhysicsSystemTests passed");
    return EXIT_SUCCESS;
}