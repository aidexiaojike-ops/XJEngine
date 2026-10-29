#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJScene.h"
#include "ECS/XJEntity.h"

#include <glm/gtx/euler_angles.hpp>

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace
{
    using namespace XJ;

    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    void CheckSuccess(
        XJTransformStatus status,
        std::string_view message)
    {
        Check(status == XJTransformStatus::Success, message);
    }

    bool Near(
        const glm::dvec3& a,
        const glm::dvec3& b,
        double tolerance = 1.0e-4)
    {
        return glm::length(a - b) <= tolerance;
    }

    bool Near(
        const glm::dmat4& a,
        const glm::dmat4& b,
        double tolerance = 1.0e-5)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (!std::isfinite(a[column][row]) ||
                    !std::isfinite(b[column][row]) ||
                    std::abs(
                        a[column][row] -
                        b[column][row]) > tolerance)
                {
                    return false;
                }
            }
        }

        return true;
    }

    void TestEulerConvention()
    {
        XJTransformComponent transform;

        const glm::dvec3 angles{17.0, 31.0, -29.0};

        CheckSuccess(
            transform.SetLocalEulerAngles(angles),
            "Euler assignment succeeds");

        const glm::dvec3 radians = glm::radians(angles);

        const glm::dmat4 expected =
            glm::eulerAngleXYZ(
                radians.x, radians.y, radians.z);

        Check(
            Near(
                glm::dmat4{transform.GetLocalMatrix()},
                expected),
            "Euler convention remains Rx Ry Rz");

        const glm::dvec3 roundTrip =
            XJTransformComponent::EulerXYZFromRotation(
                glm::dquat{transform.GetLocalRotation()});

        const glm::dmat4 rebuilt = glm::mat4_cast(
            XJTransformComponent::RotationFromEulerXYZ(
                roundTrip));

        Check(
            Near(rebuilt, expected),
            "Euler round trip preserves orientation");
    }

    void TestHierarchy()
    {
        XJScene scene;

        auto* parent = scene.CreateEntityWithTransform("Parent");
        auto* child = scene.CreateEntityWithTransform("Child");

        auto& p = parent->GetComponent<XJTransformComponent>();
        auto& c = child->GetComponent<XJTransformComponent>();

        CheckSuccess(
            p.SetLocalTRSEuler(
                {10.0, 20.0, 30.0},
                {15.0, 30.0, -20.0},
                {2.0, 3.0, 4.0}),
            "parent setup");

        CheckSuccess(
            c.SetLocalTRSEuler(
                {1.0, 2.0, 3.0},
                {17.0, 31.0, -29.0},
                {1.0, 1.0, 1.0}),
            "child setup");

        parent->XJAddChild(child);

        XJTransformWorldState world;

        CheckSuccess(c.EvaluateWorld(world), "world evaluation");

        const glm::dmat4 expected =
            glm::dmat4{p.GetLocalMatrix()} *
            glm::dmat4{c.GetLocalMatrix()};

        Check(
            Near(world.Matrix, expected, 1.0e-4),
            "world matrix inherits parent");

        CheckSuccess(
            p.SetLocalPosition({40.0, 50.0, 60.0}),
            "parent move");

        CheckSuccess(
            c.EvaluateWorld(world),
            "world reads parent change immediately");

        Check(
            Near(
                world.Matrix,
                glm::dmat4{p.GetLocalMatrix()} *
                    glm::dmat4{c.GetLocalMatrix()},
                1.0e-4),
            "no stale world data");
    }

    void TestWorldWrites()
    {
        XJScene scene;

        auto* parent = scene.CreateEntityWithTransform("Parent");
        auto* child = scene.CreateEntityWithTransform("Child");

        parent->XJAddChild(child);

        auto& p = parent->GetComponent<XJTransformComponent>();
        auto& c = child->GetComponent<XJTransformComponent>();

        CheckSuccess(
            p.SetLocalTRSEuler(
                {10.0, 20.0, 30.0},
                {15.0, 30.0, -20.0},
                {2.0, 3.0, 4.0}),
            "parent setup");

        CheckSuccess(
            c.TrySetWorldPosition({40.0, -5.0, 9.0}),
            "world position write");

        glm::dvec3 position;

        CheckSuccess(
            c.ReadWorldPosition(position),
            "world position read");

        Check(
            Near(position, {40.0, -5.0, 9.0}),
            "world position matches target");

        CheckSuccess(
            c.SetLocalEulerAngles({20.0, 35.0, -15.0}),
            "child rotation");

        CheckSuccess(
            c.SetLocalScale({-1.0, 1.0, 1.0}),
            "local reflection");

        CheckSuccess(
            c.TrySetWorldScale({5.0, 6.0, 7.0}),
            "world scale write");

        glm::dvec3 scale;

        CheckSuccess(
            c.ReadLossyWorldScale(scale),
            "world scale read");

        Check(
            Near(scale, {5.0, 6.0, 7.0}),
            "lossy scale matches target");

        Check(
            c.GetLocalScale().x < 0.0f,
            "world scale preserves reflection");

        const auto oldPosition = c.GetLocalPosition();
        const auto oldRotation = c.GetLocalRotation();
        const auto oldScale = c.GetLocalScale();
        const auto oldMatrix = c.GetLocalMatrix();

        CheckSuccess(
            p.SetLocalScale({0.0, 0.0, 0.0}),
            "singular parent setup");

        Check(
            c.TrySetWorldPosition({1.0, 2.0, 3.0}) ==
                XJTransformStatus::NonInvertibleParent,
            "singular parent rejects world position");

        Check(
            c.GetLocalPosition() == oldPosition &&
            c.GetLocalRotation() == oldRotation &&
            c.GetLocalScale() == oldScale &&
            c.GetLocalMatrix() == oldMatrix,
            "failed write preserves all local state");
    }

    void TestReparent()
    {
        XJScene scene;

        auto* a = scene.CreateEntityWithTransform("A");
        auto* b = scene.CreateEntityWithTransform("B");
        auto* child = scene.CreateEntityWithTransform("Child");

        CheckSuccess(
            a->GetComponent<XJTransformComponent>()
                .SetLocalPosition({10.0, 0.0, 0.0}),
            "A setup");

        CheckSuccess(
            b->GetComponent<XJTransformComponent>()
                .SetLocalPosition({20.0, 0.0, 0.0}),
            "B setup");

        auto& transform =
            child->GetComponent<XJTransformComponent>();

        CheckSuccess(
            transform.SetLocalPosition({1.0, 0.0, 0.0}),
            "child setup");

        a->XJAddChild(child);

        glm::dvec3 position;

        CheckSuccess(
            transform.ReadWorldPosition(position),
            "world position under A");

        Check(
            Near(position, {11.0, 0.0, 0.0}),
            "child inherits A");

        b->XJAddChild(child);

        CheckSuccess(
            transform.ReadWorldPosition(position),
            "world position after reparent");

        Check(
            Near(position, {21.0, 0.0, 0.0}),
            "reparent immediately uses B");

        Check(
            Near(
                glm::dvec3{transform.GetLocalPosition()},
                {1.0, 0.0, 0.0}),
            "reparent preserves local position");

        b->XJRemoveChild(child);

        CheckSuccess(
            transform.ReadWorldPosition(position),
            "world position after detach");

        Check(
            Near(position, {1.0, 0.0, 0.0}),
            "detached child uses local as world");
    }
    void TestInvalidWritesAreAtomic()
    {
        XJTransformComponent transform;
        CheckSuccess(transform.SetLocalTRSEuler(
            {1, 2, 3}, {17, 31, -29}, {-2, 3, 4}), "initial TRS");

        const auto position = transform.GetLocalPosition();
        const auto rotation = transform.GetLocalRotation();
        const auto scale = transform.GetLocalScale();
        const auto matrix = transform.GetLocalMatrix();
        const auto unchanged = [&]
        {
            Check(transform.GetLocalPosition() == position &&
                  transform.GetLocalRotation() == rotation &&
                  transform.GetLocalScale() == scale &&
                  transform.GetLocalMatrix() == matrix,
                  "rejected writes preserve all local state");
        };
        const double infinity = std::numeric_limits<double>::infinity();
        const double nan = std::numeric_limits<double>::quiet_NaN();

        for (const double invalid : {infinity, nan})
        {
            Check(transform.SetLocalPosition({invalid, 0, 0}) ==
                  XJTransformStatus::InvalidArgument, "non-finite position rejected");
            unchanged();
            Check(transform.SetLocalScale({1, invalid, 1}) ==
                  XJTransformStatus::InvalidArgument, "non-finite scale rejected");
            unchanged();
            Check(transform.SetLocalEulerAngles({0, invalid, 0}) ==
                  XJTransformStatus::InvalidArgument, "non-finite Euler rejected");
            unchanged();
            Check(transform.SetLocalRotation({1, invalid, 0, 0}) ==
                  XJTransformStatus::InvalidArgument, "non-finite quaternion rejected");
            unchanged();
        }
        Check(transform.SetLocalRotation({0, 0, 0, 0}) ==
              XJTransformStatus::InvalidArgument, "zero quaternion is an invalid argument");
        unchanged();

        for (const double invalid : {1.0e300, 1.0e-300})
        {
            Check(transform.SetLocalPosition({invalid, 0, 0}) ==
                  XJTransformStatus::NonFiniteResult, "float position range checked before conversion");
            unchanged();
            Check(transform.SetLocalScale({1, invalid, 1}) ==
                  XJTransformStatus::NonFiniteResult, "float scale range checked before conversion");
            unchanged();
        }
        Check(transform.TrySetWorldScale({-1, 1, 1}) ==
              XJTransformStatus::InvalidArgument, "negative world scale rejected");
        unchanged();

        for (const double magnitude : {1.0e300, 1.0e-300})
        {
            CheckSuccess(transform.SetLocalRotation({magnitude, magnitude, 0, 0}),
                         "extreme finite quaternion normalized safely");
            Check(std::abs(glm::length(transform.GetLocalRotation()) - 1.0f) < 1.0e-6f,
                  "stored quaternion is normalized");
        }
    }

    void TestHierarchyChangesAndErrors()
    {
        XJScene scene;
        auto* ancestor = scene.CreateEntityWithTransform("Ancestor");
        auto* middle = scene.CreateEntity("NoTransform");
        auto* child = scene.CreateEntityWithTransform("Child");
        ancestor->XJAddChild(middle);
        middle->XJAddChild(child);
        auto& c = child->GetComponent<XJTransformComponent>();
        CheckSuccess(ancestor->GetComponent<XJTransformComponent>().SetLocalPosition({10, 0, 0}),
                     "ancestor position");
        glm::dvec3 position;
        CheckSuccess(c.ReadWorldPosition(position), "read through transformless middle");
        Check(Near(position, {10, 0, 0}), "transformless middle is identity");

        CheckSuccess(middle->AddComponent<XJTransformComponent>().SetLocalPosition({2, 0, 0}),
                     "add middle transform");
        CheckSuccess(c.ReadWorldPosition(position), "read after component addition");
        Check(Near(position, {12, 0, 0}), "added transform immediately inherited");
        middle->AddComponent<XJTransformComponent>();
        CheckSuccess(c.ReadWorldPosition(position), "read after component replacement");
        Check(Near(position, {10, 0, 0}), "replacement cannot reuse stale world state");
        middle->RemoveComponent<XJTransformComponent>();
        CheckSuccess(c.ReadWorldPosition(position), "read after component removal");
        Check(Near(position, {10, 0, 0}), "removed transform is identity");

        auto* originalParent = ancestor->XJGetParent();
        ancestor->XJSetParent(child);
        XJTransformWorldState output;
        output.Matrix = glm::dmat4{7.0};
        const auto status = c.EvaluateWorld(output);
        const auto writeStatus = c.TrySetWorldPosition({1, 2, 3});
        ancestor->XJSetParent(originalParent);
        Check(status == XJTransformStatus::InvalidHierarchy &&
              writeStatus == XJTransformStatus::InvalidHierarchy,
              "cycles reject reads and writes");
        Check(output.Matrix == glm::dmat4{7.0}, "failed world read preserves output");
    }

    void TestWorldRotationAndCollapsedScale()
    {
        XJScene scene;
        auto* parent = scene.CreateEntityWithTransform("Parent");
        auto* child = scene.CreateEntityWithTransform("Child");
        parent->XJAddChild(child);
        auto& p = parent->GetComponent<XJTransformComponent>();
        auto& c = child->GetComponent<XJTransformComponent>();
        CheckSuccess(p.SetLocalTRSEuler({0, 0, 0}, {13, 27, -11}, {0, 0, 0}),
                     "rotated collapsed parent");
        const auto expected = XJTransformComponent::RotationFromEulerXYZ({21, 32, -17});
        CheckSuccess(c.TrySetWorldRotation(expected), "world rotation ignores singular scale");
        glm::dquat actual;
        CheckSuccess(c.ReadWorldRotation(actual), "world rotation read");
        Check(Near(glm::mat4_cast(actual), glm::mat4_cast(expected)),
              "world rotation matches target without matrix decomposition");
        CheckSuccess(c.TrySetWorldScale({0, 0, 0}), "collapsed axes can stay zero");
        Check(c.TrySetWorldScale({1, 0, 0}) == XJTransformStatus::NonInvertibleParent,
              "collapsed axis cannot reach nonzero target");
    }

    void TestExtremeParentScales()
    {
        for (const double magnitude : {1.0e30, 1.0e-30})
        {
            XJScene scene;
            auto* child = scene.CreateEntityWithTransform("Child");
            auto* current = child;
            for (int level = 0; level < 8; ++level)
            {
                auto* parent = scene.CreateEntityWithTransform("Parent");
                CheckSuccess(parent->GetComponent<XJTransformComponent>().SetLocalScale(
                    {magnitude, magnitude, magnitude}), "extreme parent scale");
                parent->XJAddChild(current);
                current = parent;
            }
            auto& c = child->GetComponent<XJTransformComponent>();
            glm::dvec3 worldScale;
            CheckSuccess(c.ReadLossyWorldScale(worldScale), "stable world axis lengths");
            Check(std::isfinite(worldScale.x) && worldScale.x > 0,
                  "axis length avoids intermediate overflow and underflow");
            CheckSuccess(c.TrySetWorldScale(worldScale), "extreme world scale round trip");
            CheckSuccess(c.TrySetWorldPosition(worldScale), "normalized parent inverse remains usable");
            Check(Near(glm::dvec3{c.GetLocalPosition()}, {1, 1, 1}),
                  "extreme parent inverse recovers local position");
        }
    }

    void TestWorldOverflowIsAtomic()
    {
        XJScene scene;
        auto* child = scene.CreateEntityWithTransform("Child");
        auto* current = child;
        for (int level = 0; level < 10; ++level)
        {
            auto* parent = scene.CreateEntityWithTransform("LargeParent");
            CheckSuccess(parent->GetComponent<XJTransformComponent>().SetLocalScale({1.0e30, 1.0e30, 1.0e30}),
                         "large parent setup");
            parent->XJAddChild(current);
            current = parent;
        }
        auto& transform = child->GetComponent<XJTransformComponent>();
        // Local 写入只验证本地可表示性，允许世界结果暂时超出 double。
        CheckSuccess(transform.SetLocalPosition({1.0e30, 0, 0}), "finite local position");
        const auto oldRotation = transform.GetLocalRotation();
        const auto oldMatrix = transform.GetLocalMatrix();
        Check(transform.TrySetWorldRotation({1, 0, 0, 0}) == XJTransformStatus::NonFiniteResult,
              "world write validates final complete matrix");
        Check(transform.GetLocalRotation() == oldRotation && transform.GetLocalMatrix() == oldMatrix,
              "overflowing world write leaves local state untouched");

        glm::dvec3 output{7, 8, 9};
        Check(transform.ReadWorldPosition(output) == XJTransformStatus::NonFiniteResult,
              "overflowing world read fails");
        Check(output == glm::dvec3{7, 8, 9}, "failed position read preserves output");
    }
}

int main()
{
    TestEulerConvention();
    TestHierarchy();
    TestWorldWrites();
    TestReparent();
    TestInvalidWritesAreAtomic();
    TestHierarchyChangesAndErrors();
    TestWorldRotationAndCollapsedScale();
    TestExtremeParentScales();
    TestWorldOverflowIsAtomic();

    spdlog::info("Transform component tests passed.");
    return EXIT_SUCCESS;
}
