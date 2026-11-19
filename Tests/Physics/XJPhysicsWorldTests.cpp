#include "Physics/XJPhysicsWorld.h"

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

    // 阶段 1 的核心验收：静态地面 + 动态箱子，120 步后落地并静止。
    void TestBoxFallsAndRests()
    {
        XJ::XJPhysicsWorld world;
        Check(world.Initialize({}) == XJ::XJPhysicsResult::Success, "world initializes");
        Check(world.IsInitialized(), "world reports initialized");

        XJ::XJPhysicsBodyId ground;
        Check(world.CreateStaticPlane(glm::dvec3(0.0, 1.0, 0.0), glm::dvec3(0.0), ground)
                == XJ::XJPhysicsResult::Success, "ground plane created");

        constexpr double kHalfExtent = 0.5;
        constexpr double kStartHeight = 5.0;

        XJ::XJPhysicsBodyId box;
        Check(world.CreateDynamicBox(glm::dvec3(kHalfExtent),
                  glm::dvec3(0.0, kStartHeight, 0.0),
                  glm::dquat(1.0, 0.0, 0.0, 0.0), 1.0, box)
                == XJ::XJPhysicsResult::Success, "dynamic box created");

        Check(world.GetBodyCount() == 2, "world tracks two bodies");

        for (int i = 0; i < 120; ++i)
        {
            Check(world.Step(kFixedDeltaTime) == XJ::XJPhysicsResult::Success,
                "step succeeds");
        }

        glm::dvec3 position{};
        glm::dquat rotation{};
        Check(world.GetBodyPose(box, position, rotation), "box pose readable");

        Check(std::abs(position.y - kHalfExtent) < 0.05, "box rests on ground");
        Check(std::abs(position.x) < 0.05, "box does not drift on X");
        Check(std::abs(position.z) < 0.05, "box does not drift on Z");

        glm::dvec3 velocity{};
        Check(world.GetLinearVelocity(box, velocity), "box velocity readable");
        Check(glm::length(velocity) < 0.01, "box comes to rest");

        Check(!world.IsBodyActive(box), "box went to sleep");

        Check(world.DestroyBody(box), "box destroyed");
        Check(world.DestroyBody(ground), "ground destroyed");
        Check(world.GetBodyCount() == 0, "no bodies left");

        world.Shutdown();
        Check(!world.IsInitialized(), "world shut down");
    }

    // 模拟重复 Play/Stop：反复建世界、跑、销毁，检查全局运行时能正确收发。
    void TestRepeatedInitializeAndShutdown()
    {
        for (int round = 0; round < 5; ++round)
        {
            XJ::XJPhysicsWorld world;
            Check(world.Initialize({}) == XJ::XJPhysicsResult::Success,
                "world initializes on every round");

            XJ::XJPhysicsBodyId ground;
            XJ::XJPhysicsBodyId sphere;

            Check(world.CreateStaticPlane(glm::dvec3(0.0, 1.0, 0.0), glm::dvec3(0.0), ground)
                    == XJ::XJPhysicsResult::Success, "ground created on every round");

            Check(world.CreateDynamicSphere(0.5, glm::dvec3(0.0, 3.0, 0.0),
                      glm::dquat(1.0, 0.0, 0.0, 0.0), 1.0, sphere)
                    == XJ::XJPhysicsResult::Success, "sphere created on every round");

            for (int i = 0; i < 30; ++i)
                world.Step(kFixedDeltaTime);

            world.Shutdown();
        }
    }

    void TestInvalidInputRejected()
    {
        XJ::XJPhysicsWorld world;
        XJ::XJPhysicsBodyId id;
        glm::dvec3 position{};
        glm::dquat rotation{};

        Check(world.CreateDynamicBox(glm::dvec3(0.5), glm::dvec3(0.0),
                  glm::dquat(1.0, 0.0, 0.0, 0.0), 1.0, id)
                == XJ::XJPhysicsResult::NotInitialized,
            "creation before init fails");

        Check(!world.GetBodyPose(id, position, rotation), "invalid handle has no pose");
        Check(!world.DestroyBody(id), "invalid handle cannot be destroyed");

        Check(world.Initialize({}) == XJ::XJPhysicsResult::Success, "world initializes");
        Check(world.Initialize({}) == XJ::XJPhysicsResult::AlreadyInitialized,
            "double initialize rejected");

        Check(world.Step(0.0) == XJ::XJPhysicsResult::InvalidArgument, "zero dt rejected");
        Check(world.Step(-1.0) == XJ::XJPhysicsResult::InvalidArgument, "negative dt rejected");

        Check(world.CreateDynamicBox(glm::dvec3(0.0), glm::dvec3(0.0),
                  glm::dquat(1.0, 0.0, 0.0, 0.0), 1.0, id)
                == XJ::XJPhysicsResult::ShapeCreationFailed,
            "zero half extent rejected");

        Check(world.CreateDynamicBox(glm::dvec3(0.5), glm::dvec3(0.0),
                  glm::dquat(1.0, 0.0, 0.0, 0.0), -1.0, id)
                == XJ::XJPhysicsResult::InvalidArgument,
            "negative mass rejected");

        Check(world.CreateStaticPlane(glm::dvec3(0.0), glm::dvec3(0.0), id)
                == XJ::XJPhysicsResult::ShapeCreationFailed,
            "zero plane normal rejected");

        world.Shutdown();
    }
}

int main()
{
    TestBoxFallsAndRests();
    TestRepeatedInitializeAndShutdown();
    TestInvalidInputRejected();

    spdlog::info("XJPhysicsWorldTests passed");
    return EXIT_SUCCESS;
}