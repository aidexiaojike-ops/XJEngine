#include "Asset/Instantiation/XJSceneInstantiator.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include "Camera/XJCameraController.h"
#include "Camera/XJCameraMath.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "Geometry/XJBoundingBox.h"
#include "Render/XJLightSceneUtils.h"
#include "Services/XJEditorSceneService.h"

#include <nlohmann/json.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>

namespace
{
    using namespace XJ;

    void Check(bool value, std::string_view message)
    {
        if (!value)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    void Success(XJTransformStatus status, std::string_view message)
    {
        Check(status == XJTransformStatus::Success, message);
    }

    bool Near(glm::dvec3 a, glm::dvec3 b, double tolerance = 1.0e-4)
    {
        return glm::length(a - b) <= tolerance;
    }

    bool Near(const glm::dmat4& a, const glm::dmat4& b, double tolerance = 1.0e-4)
    {
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (!std::isfinite(a[column][row]) || !std::isfinite(b[column][row]) ||
                    std::abs(a[column][row] - b[column][row]) > tolerance) return false;
        return true;
    }

    void TestInspectorTransformEditing()
    {
        using Space = XJEditorTransformSpace;
        using Fields = XJEditorTransformFields;
        using Outcome = XJEditorTransformEditOutcome;

        XJScene scene;

        auto* parent =
            scene.CreateEntityWithTransform("Parent");

        auto* child =
            scene.CreateEntityWithTransform("Child");

        parent->XJAddChild(child);

        auto& p =
            parent->GetComponent<XJTransformComponent>();

        auto& c =
            child->GetComponent<XJTransformComponent>();

        Success(
            p.SetLocalTRSEuler(
                {10, 0, 0},
                {0, 0, 0},
                {2, 3, 4}),
            "Inspector parent setup");

        Success(
            c.SetLocalTRSEuler(
                {1, 2, 3},
                {17, 31, -29},
                {-1, 2, 3}),
            "Inspector child setup");

        const auto id =
            static_cast<XJEditorEntityId>(
                child->XJGetUUID());

        const auto view =
            XJEditorSceneService::BuildTransformView(
                scene, id);

        Check(
            view.Valid && view.World.Valid,
            "Inspector publishes both coordinate spaces");

        Check(
            view.Position == glm::vec3{1, 2, 3} &&
            Near(view.World.Position, {12, 6, 12}),
            "Inspector keeps Local and World separate");

        const auto originalRotation =
            c.GetLocalRotation();

        const auto originalScale =
            c.GetLocalScale();

        XJEditorUpdateTransformRequest request;

        request.EntityId = id;
        request.Space = Space::World;
        request.Fields = Fields::Position;
        request.Position = {20, 9, 16};

        // 未选择的字段必须完全忽略。
        const double nan =
            std::numeric_limits<double>::quiet_NaN();

        request.Rotation = {nan, nan, nan};
        request.Scale = {nan, nan, nan};

        auto result =
            XJEditorSceneService::UpdateTransform(
                scene, request);

        Check(
            result.Outcome == Outcome::Applied,
            "Inspector applies selected World field");

        glm::dvec3 position;

        Success(
            c.ReadWorldPosition(position),
            "Inspector edited World position read");

        Check(
            Near(position, request.Position),
            "Inspector World position reaches target");

        Check(
            c.GetLocalRotation() == originalRotation &&
            c.GetLocalScale() == originalScale,
            "position edit preserves rotation and scale");

        result =
            XJEditorSceneService::UpdateTransform(
                scene, request);

        Check(
            result.Outcome == Outcome::Unchanged,
            "identical Inspector edit is unchanged");

        const auto oldPosition = c.GetLocalPosition();
        const auto oldRotation = c.GetLocalRotation();
        const auto oldScale = c.GetLocalScale();
        const auto oldMatrix = c.GetLocalMatrix();

        request.Fields =
            Fields::Position | Fields::Scale;

        request.Position = {100, 200, 300};
        request.Scale = {-1, 2, 3};

        result =
            XJEditorSceneService::UpdateTransform(
                scene, request);

        Check(
            result.Rejected() && !result.Error.empty(),
            "invalid combined edit reports failure");

        Check(
            c.GetLocalPosition() == oldPosition &&
            c.GetLocalRotation() == oldRotation &&
            c.GetLocalScale() == oldScale &&
            c.GetLocalMatrix() == oldMatrix,
            "combined edit failure is atomic");

        Success(
            p.SetLocalScale({0, 0, 0}),
            "Inspector singular parent");

        request.Fields = Fields::Position;
        request.Position = {1, 2, 3};

        result =
            XJEditorSceneService::UpdateTransform(
                scene, request);

        Check(
            result.Rejected(),
            "Inspector rejects World position under singular parent");

        request.Space = Space::Local;
        request.Position = {4, 5, 6};

        result =
            XJEditorSceneService::UpdateTransform(
                scene, request);

        Check(
            result.Changed() &&
            c.GetLocalPosition() == glm::vec3{4, 5, 6},
            "Local editing remains available under singular parent");

        const auto refreshed =
            XJEditorSceneService::BuildTransformView(
                scene, id);

        Check(
            refreshed.Valid && refreshed.World.Valid &&
            Near(refreshed.World.Position, {10, 0, 0}),
            "Transform snapshot reflects current parent state");
    }

    void TestCameraViewFailure()
    {
        XJScene scene;
        auto* entity = scene.CreateEntityWithTransform("Camera validation");
        auto& camera = entity->AddComponent<XJCameraComponent>();
        Success(entity->GetComponent<XJTransformComponent>().SetLocalPosition({8, 10, 8}),
            "camera validation setup");
        glm::mat4 view;
        Check(camera.TryGetViewMatrix(view), "valid camera returns current view");
        Check(Near(view, camera.XJGetViewMatrix()), "explicit view agrees with legacy getter");
        const auto lastValid = view;

        entity->RemoveComponent<XJTransformComponent>();
        Check(!camera.TryGetViewMatrix(view) && Near(view, lastValid),
            "missing Transform rejects view without returning cached pose");

        auto* parent = scene.CreateEntityWithTransform("Large parent");
        parent->XJAddChild(entity);
        auto& transform = entity->AddComponent<XJTransformComponent>();
        const double large = double(std::numeric_limits<float>::max()) * 0.75;
        Success(parent->GetComponent<XJTransformComponent>().SetLocalPosition({large, 0, 0}),
            "large camera parent position");
        Success(transform.SetLocalPosition({large, 0, 0}), "large camera local position");
        Check(!camera.TryGetViewMatrix(view) && Near(view, lastValid),
            "view outside float range rejects instead of using cached pose");

        Success(parent->GetComponent<XJTransformComponent>().SetLocalPosition({0, 0, 0}),
            "restore camera parent position");
        Success(transform.SetLocalPosition({1, 2, 3}), "restore camera local position");
        Check(camera.TryGetViewMatrix(view) && !Near(view, lastValid),
            "valid camera recovers with fresh view after failure");

        // Build a finite local hierarchy whose composed world matrix overflows double.
        auto* ancestor = parent;
        for (int i = 0; i < 10; ++i)
        {
            auto* next = scene.CreateEntityWithTransform("Overflow ancestor");
            Success(next->GetComponent<XJTransformComponent>().SetLocalScale({large, large, large}),
                "overflow ancestor scale");
            next->XJAddChild(ancestor);
            ancestor = next;
        }
        const auto beforeFailure = view;
        Check(!camera.TryGetViewMatrix(view) && Near(view, beforeFailure),
            "world evaluation failure rejects view without changing output");

        XJCameraComponent detached;
        Check(!detached.TryGetViewMatrix(view) && Near(view, beforeFailure),
            "camera without owner rejects view instead of returning identity");
    }

    void TestCamera()
    {
        XJScene scene;
        auto* parent = scene.CreateEntityWithTransform("Parent");
        auto* entity = scene.CreateEntityWithTransform("Camera");
        parent->XJAddChild(entity);
        auto& p = parent->GetComponent<XJTransformComponent>();
        auto& t = entity->GetComponent<XJTransformComponent>();
        auto& camera = entity->AddComponent<XJCameraComponent>();
        Success(p.SetLocalTRSEuler({10, 20, 30}, {12, 27, -13}, {-2, 3, 4}), "camera parent setup");
        Success(t.SetLocalTRSEuler({1, 2, 3}, {17, -21, 33}, {2, 0, 4}), "camera local setup");
        XJTransformWorldState world;
        Success(t.EvaluateWorld(world), "camera world evaluation");
        const auto localMatrix = t.GetLocalMatrix();
        const auto localRotation = t.GetLocalRotation();
        const auto parentMatrix = p.GetLocalMatrix();
        const glm::dmat4 expected = glm::mat4_cast(glm::conjugate(world.Rotation)) *
            glm::translate(glm::dmat4{1}, -glm::dvec3{world.Matrix[3]});
        for (auto mode : {CameraMode::Free, CameraMode::Orbit})
        {
            camera.XJSetCameraMode(mode);
            camera.XJSetTarget({9, 8, 7});
            camera.XJSetRadius(11);
            Check(Near(glm::dmat4{camera.XJGetViewMatrix()}, expected),
                  "camera view uses world position and pure rotation, ignoring scale");
            camera.XJGetViewMatrix();
            Check(t.GetLocalMatrix() == localMatrix && t.GetLocalRotation() == localRotation &&
                  p.GetLocalMatrix() == parentMatrix && camera.XJGetRadius() == 11 &&
                  camera.XJGetTarget() == glm::vec3{9, 8, 7}, "camera Getter does not mutate ECS or orbit metadata");
        }
        Check(Near(glm::dvec3{camera.XJGetPosition()}, glm::dvec3{world.Matrix[3]}),
              "camera position getter has no separate source of truth");

        camera.XJSetCameraMode(CameraMode::Free);
        XJCameraController controller;
        const auto initialPosition = glm::dvec3{world.Matrix[3]};
        controller.OnMouseScroll(1, entity);
        glm::dvec3 actualPosition;
        Success(t.ReadWorldPosition(actualPosition), "scroll position read");
        Check(Near(actualPosition, initialPosition + world.Rotation * glm::dvec3{0, 0, -0.5}),
              "free scroll moves along world camera forward under parent");

        camera.XJSetCameraMode(CameraMode::Orbit);
        camera.XJSetTarget({3, 4, 5});
        camera.XJSetRadius(6);
        Success(t.TrySetWorldPose({3, 4, 11}, {1, 0, 0, 0}), "orbit pose setup");
        controller.OnMouseScroll(1, entity);
        Success(t.ReadWorldPosition(actualPosition), "orbit position read");
        Check(Near(actualPosition, {3, 4, 10.7}) && std::abs(camera.XJGetRadius() - 5.7f) < 1.0e-5,
              "orbit controller applies radius to pose without calling view Getter");
        const auto beforeFailure = t.GetLocalMatrix();
        Success(p.SetLocalScale({0, 0, 0}), "singular camera parent");
        controller.OnMouseScroll(1, entity);
        Check(t.GetLocalMatrix() == beforeFailure && std::abs(camera.XJGetRadius() - 5.7f) < 1.0e-5,
              "failed orbit request preserves pose and radius");
    }

    void TestLightPickingAndInspector()
    {
        XJScene scene;
        auto* parent = scene.CreateEntityWithTransform("Parent");
        auto* entity = scene.CreateEntityWithTransform("Light");
        parent->XJAddChild(entity);
        auto& p = parent->GetComponent<XJTransformComponent>();
        auto& t = entity->GetComponent<XJTransformComponent>();
        auto& light = entity->AddComponent<XJLightComponent>();
        light.XJSetEnable(true);
        light.XJSetLightType(XJLightType::Point);
        Success(p.SetLocalTRSEuler({10, 0, 0}, {0, 0, 90}, {2, 3, 4}), "light parent setup");
        Success(t.SetLocalPosition({1, 0, 0}), "light local setup");
        glm::vec3 position;
        Success(t.ReadWorldPosition(position), "light world read");
        Check(Near(glm::dvec3{position}, {10, 2, 0}), "light inherits position");
        XJLightUbo ubo;
        XJLightSceneUtils::BuildLightUboFromScene(scene, ubo);
        Check(ubo.PointCount == 1 && Near(glm::dvec3{ubo.Points[0].PositionIntensity}, {10, 2, 0}),
              "light GPU data uses world position");
        Check(Near(glm::dvec3{XJLightSceneUtils::BuildDirection(t)}, {0, 1, 0}),
              "light direction inherits rotation and retains local +X convention");
        XJEditorSceneRaycastHit hit;
        Check(XJEditorSceneService::RaycastClosestSceneEntity(scene, {10, 2, 5}, {0, 0, -1}, 20, hit) &&
              hit.Entity == static_cast<XJEditorEntityId>(entity->XJGetUUID()),
              "light gizmo picking uses rendered world position");
        Check(!XJEditorSceneService::RaycastClosestSceneEntity(scene, {1, 0, 5}, {0, 0, -1}, 20, hit),
              "light is not picked at its old local position");

        auto details = XJEditorSceneService::BuildEntityDetailsView(scene,
            static_cast<XJEditorEntityId>(entity->XJGetUUID()), nullptr, {});
        Check(details.Transform.Position == glm::vec3{1, 0, 0}, "Inspector displays local position");
        XJEditorUpdateTransformRequest request;
        request.EntityId = static_cast<XJEditorEntityId>(entity->XJGetUUID());
        request.Position = {2, 0, 0};
        request.Rotation = {0, 0, 0};
        request.Scale = {1, 1, 1};
        XJEditorSceneService::UpdateTransform(scene, request);
        Success(t.ReadWorldPosition(position), "Inspector update read");
        Check(Near(glm::dvec3{position}, {10, 4, 0}), "Inspector applies local edits through Transform");

        glm::mat4 matrix;
        Success(t.ReadWorldMatrix(matrix), "float render matrix read");
        XJBoundingBox localBounds;
        localBounds.Min = {-1, -1, -1};
        localBounds.Max = {1, 1, 1};
        const auto bounds = localBounds.Transformed(matrix);
        Check(Near(glm::dvec3{(bounds.Min + bounds.Max) * 0.5f}, {10, 4, 0}),
              "world bounds and world light data share the same transform");
    }

    void TestSceneMigration(const std::filesystem::path& root)
    {
        for (const int version : {2, 3})
        {
            nlohmann::json json = {
                {"version", version}, {"objects", nlohmann::json::array({
                    {{"uuid", "101"}, {"type", "Entity"}, {"name", "Parent"},
                     {"components", {{"transform", {{"pos", {10, 20, 30}}, {"rot", {15, 31, -19}}, {"scale", {-2, 3, 4}}}}}}},
                    {{"uuid", "102"}, {"type", "Entity"}, {"name", "Camera"}, {"parent", "101"},
                     {"components", {{"transform", {{"pos", {4, 5, 6}}, {"rot", {35, 20, 7}}, {"scale", {1, 1, 1}}}},
                                     {"camera", {{"enabled", true}}}}}}
                })}
            };
            const auto path = root / ("Legacy" + std::to_string(version) + ".xjscene");
            { std::ofstream file(path); file << json; Check(file.good(), "legacy fixture written"); }
            const auto asset = XJSceneAssetSerializer::LoadFromFile(path);
            Check(asset != nullptr, "legacy scene migrated");
            XJScene scene;
            Check(XJSceneInstantiator::Instantiate(*asset, scene), "migrated scene instantiated");
            auto* entity = scene.FindEntityByUUID(XJUUID{102});
            auto& t = entity->GetComponent<XJTransformComponent>();
            glm::dvec3 position;
            glm::dquat rotation;
            Success(t.ReadWorldPosition(position), "migrated world position");
            Success(t.ReadWorldRotation(rotation), "migrated world rotation");
            Check(Near(position, {4, 5, 6}), "legacy camera keeps world position despite new parent inheritance");
            Check(Near(rotation * glm::dvec3{0, 0, -1},
                glm::dvec3{CameraMath::BuildForwardFromYawPitch(35, 20)}), "legacy camera keeps old yaw/pitch view direction");

            auto& camera = entity->GetComponent<XJCameraComponent>();
            camera.XJSetCameraMode(CameraMode::Orbit);
            camera.XJSetTarget({7, 8, 9});
            camera.XJSetRadius(12);
            const auto saved = XJSceneAssetSerializer::BuildFromScene(scene);
            const auto savedPath = root / ("New" + std::to_string(version) + ".xjscene");
            Check(XJSceneAssetSerializer::SaveToFile(*saved, savedPath), "v4 scene saved");
            { std::ifstream file(savedPath); nlohmann::json output; file >> output;
              Check(output["version"] == 4, "new scene version is 4"); }
            const auto reloaded = XJSceneAssetSerializer::LoadFromFile(savedPath);
            Check(reloaded != nullptr, "v4 scene reloaded");
            XJScene clone;
            Check(XJSceneInstantiator::Instantiate(*reloaded, clone), "v4 scene instantiated");
            auto* cloned = clone.FindEntityByUUID(XJUUID{102});
            glm::dvec3 clonedPosition;
            Success(cloned->GetComponent<XJTransformComponent>().ReadWorldPosition(clonedPosition), "v4 position");
            Check(Near(clonedPosition, position), "v4 round trip does not migrate camera twice");
            auto& clonedCamera = cloned->GetComponent<XJCameraComponent>();
            Check(clonedCamera.XJGetCameraMode() == CameraMode::Orbit && clonedCamera.XJGetRadius() == 12 &&
                  clonedCamera.XJGetTarget() == glm::vec3{7, 8, 9}, "orbit metadata round trips");
            std::filesystem::remove(path);
            std::filesystem::remove(savedPath);

            json["objects"][0]["components"]["transform"]["scale"] = {0, 0, 0};
            const auto singular = root / "Singular.xjscene";
            { std::ofstream file(singular); file << json; }
            Check(!XJSceneAssetSerializer::LoadFromFile(singular), "unrepresentable legacy camera migration fails safely");
            std::filesystem::remove(singular);
        }
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "opencode" /
        ("XJTransformIntegrationTests-" + std::to_string(static_cast<uint64_t>(XJ::XJUUID{})));
    std::filesystem::create_directories(root);
    TestCamera();
    TestCameraViewFailure();
    TestLightPickingAndInspector();
    TestSceneMigration(root);
    TestInspectorTransformEditing();
    std::filesystem::remove(root);
    spdlog::info("Transform integration tests passed.");
    return EXIT_SUCCESS;
}
