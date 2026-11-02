#include "Asset/XJAssetRegistry.h"
#include "Controllers/XJEditorSceneController.h"
#include "Controllers/XJEditorTransformToolsController.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "Services/XJEditorSceneService.h"
#include "UI/XJEditorUIState.h"

#include <spdlog/spdlog.h>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace
{
    using namespace XJ;
    using Space = XJEditorTransformSpace;
    using Fields = XJEditorTransformFields;

    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    bool Near(glm::dvec3 a, glm::dvec3 b)
    {
        return glm::length(a - b) < 1.0e-4;
    }

    void TestSpacesAndFieldIsolation()
    {
        XJScene scene;
        auto* parent = scene.CreateEntityWithTransform("Parent");
        auto* child = scene.CreateEntityWithTransform("Child");
        auto& p = parent->GetComponent<XJTransformComponent>();
        auto& c = child->GetComponent<XJTransformComponent>();
        const auto id = static_cast<XJEditorEntityId>(child->XJGetUUID());
        Check(c.SetLocalTRSEuler({1, 2, 3}, {17, 31, -29}, {1, 2, 3}) == XJTransformStatus::Success,
            "root setup");
        const auto rootView = XJEditorSceneService::BuildTransformView(scene, id);
        Check(rootView.World.Valid && Near(rootView.Position, rootView.World.Position) &&
            Near(rootView.Rotation, rootView.World.Rotation) && Near(rootView.Scale, rootView.World.Scale),
            "root Local and World agree with positive scale");

        parent->XJAddChild(child);
        Check(p.SetLocalTRSEuler({10, -5, 7}, {23, -18, 41}, {2, 3, 4}) == XJTransformStatus::Success,
            "nonuniform rotated parent setup");
        Check(c.SetLocalScale({-1, 2, -3}) == XJTransformStatus::Success, "reflection setup");
        const auto view = XJEditorSceneService::BuildTransformView(scene, id);
        Check(view.World.Valid && !Near(view.Position, view.World.Position), "parented spaces differ");

        XJEditorSceneController controller;
        controller.SetScene(&scene);
        XJEditorUIState state;
        XJEditorTransformToolsController tools;
        const auto matrix = c.GetLocalMatrix();
        for (auto space : {Space::World, Space::Local, Space::World})
        {
            state.TransformToolRequests.emplace_back(XJSetTransformSpace{space});
            tools.ProcessRequests(state, true);
            controller.ProcessRequests(state);
            Check(state.TransformTools.Space == space && c.GetLocalMatrix() == matrix,
                "space switch does not mutate object");
            Check(!controller.IsSceneDirty() && !controller.CanUndo(), "space switch has no Dirty or Undo");
        }

        XJEditorUpdateTransformRequest request;
        request.EntityId = id;
        request.Space = Space::World;
        request.Fields = Fields::Position;
        request.Position = {20, 9, 16};
        const auto rotation = c.GetLocalRotation();
        const auto scale = c.GetLocalScale();
        Check(XJEditorSceneService::UpdateTransform(scene, request).Changed(), "World Position applied");
        Check(c.GetLocalRotation() == rotation && c.GetLocalScale() == scale,
            "World Position preserves Local Rotation and Scale");

        const auto beforeRotation = XJEditorSceneService::BuildTransformView(scene, id);
        const auto localPosition = c.GetLocalPosition();
        request.Fields = Fields::Rotation;
        request.Rotation = {41, -12, 67};
        Check(XJEditorSceneService::UpdateTransform(scene, request).Changed(), "World Rotation applied");
        const auto afterRotation = XJEditorSceneService::BuildTransformView(scene, id);
        Check(c.GetLocalScale() == scale && c.GetLocalPosition() == localPosition,
            "World Rotation preserves Local Scale and Position");
        Check(Near(afterRotation.World.Rotation, request.Rotation) &&
            !Near(beforeRotation.World.Scale, afterRotation.World.Scale),
            "World Rotation reaches target without preserving old lossy scale");

        request.Fields = Fields::Scale;
        request.Scale = {5, 6, 7};
        const auto rotated = c.GetLocalRotation();
        Check(XJEditorSceneService::UpdateTransform(scene, request).Changed(), "World Scale applied");
        const auto scaled = XJEditorSceneService::BuildTransformView(scene, id);
        Check(Near(scaled.World.Scale, request.Scale) && c.GetLocalScale().x < 0 &&
            c.GetLocalScale().y > 0 && c.GetLocalScale().z < 0 && c.GetLocalRotation() == rotated,
            "World Scale reaches target and preserves reflection signs and rotation");
        const auto beforeReject = c.GetLocalMatrix();
        request.Scale = {-1, 2, 3};
        const auto rejected = XJEditorSceneService::UpdateTransform(scene, request);
        Check(rejected.Rejected() && !rejected.Error.empty() && c.GetLocalMatrix() == beforeReject,
            "negative World Scale rejected atomically");

        // A reflected root is an intentional exception to numeric Local/World equality.
        parent->XJRemoveChild(child);
        const auto reflectedRoot = XJEditorSceneService::BuildTransformView(scene, id);
        Check(reflectedRoot.Scale.x < 0 && reflectedRoot.World.Scale.x >= 0,
            "root World lossy scale is nonnegative even when Local scale is reflected");
    }

    void TestFailureHistoryAndFeedback()
    {
        XJScene scene;
        XJAssetRegistry registry;
        auto* parent = scene.CreateEntityWithTransform("Singular parent");
        auto* child = scene.CreateEntityWithTransform("Child");
        parent->XJAddChild(child);
        Check(parent->GetComponent<XJTransformComponent>().SetLocalScale({0, 1, 1}) ==
            XJTransformStatus::Success, "singular parent setup");
        const auto id = static_cast<XJEditorEntityId>(child->XJGetUUID());
        XJEditorSceneController controller;
        controller.SetScene(&scene);
        controller.SetAssetRegistry(&registry);
        XJEditorUIState state;
        XJEditorUpdateTransformRequest request;
        request.EntityId = id;
        request.Space = Space::World;
        request.Fields = Fields::Position;
        request.Position = {4, 5, 6};
        auto submit = [&]()
        {
            state.SceneRequests.RequestUpdateTransform = true;
            state.SceneRequests.UpdateTransform = request;
            controller.ProcessRequests(state);
        };
        submit();
        Check(!controller.IsSceneDirty() && !controller.CanUndo() &&
            !state.TransformEditFeedback.Error.empty() && state.TransformEditFeedback.EntityId == id &&
            state.TransformEditFeedback.Space == Space::World,
            "rejected World Position publishes reason without Dirty or Undo");
        request.Space = Space::Local;
        submit();
        Check(controller.IsSceneDirty() && controller.CanUndo() && state.TransformEditFeedback.Error.empty(),
            "Local Position succeeds under singular parent and creates history");
        request.Space = Space::World;
        request.Position = {9, 8, 7};
        submit();
        Check(!state.TransformEditFeedback.Error.empty(), "second invalid edit rejected");
        Check(controller.Undo(state), "Undo after failure succeeds");
        auto* restored = scene.FindEntityByUUID(XJUUID{static_cast<uint64_t>(id)});
        Check(restored && Near(restored->GetComponent<XJTransformComponent>().GetLocalPosition(), {0, 0, 0}) &&
            !controller.CanUndo() && controller.CanRedo(),
            "one Undo skips rejected edit and restores prior successful edit");
        Check(controller.Redo(state), "Redo after failure succeeds");
        restored = scene.FindEntityByUUID(XJUUID{static_cast<uint64_t>(id)});
        Check(restored && Near(restored->GetComponent<XJTransformComponent>().GetLocalPosition(), {4, 5, 6}),
            "Redo restores successful Local edit");
        submit();
        Check(!state.TransformEditFeedback.Error.empty(), "failure after Redo rejected");
        Check(controller.Undo(state), "Undo remains usable after Redo and rejection");
        submit();
        Check(controller.CanRedo(), "rejected edit preserves existing Redo history");
    }
}

int main()
{
    TestSpacesAndFieldIsolation();
    TestFailureHistoryAndFeedback();
    spdlog::info("Transform acceptance tests passed.");
    return EXIT_SUCCESS;
}
