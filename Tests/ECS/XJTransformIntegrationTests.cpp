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
    TestLightPickingAndInspector();
    TestSceneMigration(root);
    std::filesystem::remove(root);
    spdlog::info("Transform integration tests passed.");
    return EXIT_SUCCESS;
}
