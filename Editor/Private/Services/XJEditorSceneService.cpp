#include "Services/XJEditorSceneService.h".


#include "ECS/XJEntity.h"
#include "ECS/XJNode.h"
#include "ECS/XJScene.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/XJTransformComponent.h"

#include <algorithm>
#include <unordered_set>

namespace XJ
{
    namespace
    {
        XJEditorEntityView BuildEntityViewRecursive(XJEntity* entity)
        {
            XJEditorEntityView view;

            if (!entity)
                return view;

            view.Id = static_cast<XJEditorEntityId>(entity->XJGetUUID());
            view.Name = entity->XJGetName().empty() ? "XJUnnamed" : entity->XJGetName();
            //获取所有组件
            view.HasTransform = entity->HasComponent<XJTransformComponent>();
            view.HasMesh = entity->HasComponent<XJMeshAssetRefComponent>();
            view.HasCamera = entity->HasComponent<XJCameraComponent>();
            view.HasSceneRef = entity->HasComponent<XJSceneAssetRefComponent>();
            //查看信息
            if (view.HasMesh)
            {
                const auto& meshRef = entity->GetComponent<XJMeshAssetRefComponent>();
                view.MeshAsset = meshRef.Mesh.IsValid() ? meshRef.Mesh.Handle : 0;
            }

            if (view.HasSceneRef)
            {
                const auto& sceneRef = entity->GetComponent<XJSceneAssetRefComponent>();
                view.SourceSceneUri = sceneRef.SourceScene.ToUri();
            }

            for (XJNode* child : entity->XJGetChildren())
            {
                if (XJEntity* childEntity = dynamic_cast<XJEntity*>(child))
                    view.Children.push_back(BuildEntityViewRecursive(childEntity));
            }

            return view;
        }
    }

    XJEntity* XJEditorSceneService::FindEntityById(XJScene& scene, XJEditorEntityId id)
    {
        if (id == XJ_INVALID_EDITOR_ENTITY_ID)
            return nullptr;
        //循环所有ecs 通过ID找到
        for (const auto& [enttEntity, entityPtr] : scene.GetEntities())
        {
            if (!entityPtr)
                continue;

            if (static_cast<XJEditorEntityId>(entityPtr->XJGetUUID()) == id)
                return entityPtr.get();
        }

        return nullptr;
    }

    std::vector<XJEditorEntityId> XJEditorSceneService::FindEntitiesUsingAsset(XJScene& scene, XJAssetHandle assetHandle)
    {
        std::vector<XJEditorEntityId> result;

        if (assetHandle == 0)
            return result;

        auto& registry = scene.XJGetEcsRegistry();

        auto meshView = registry.view<XJMeshAssetRefComponent>();
        meshView.each([&](auto entity, XJMeshAssetRefComponent& meshRef)
        {
            if (meshRef.Mesh.Handle != assetHandle)
                return;

            XJEntity* xjEntity = scene.XJGetEntities(entity);
            if (!xjEntity)
                return;

            result.push_back(static_cast<XJEditorEntityId>(xjEntity->XJGetUUID()));
        });

        auto materialView = registry.view<XJMaterialAssetRefComponent>();
        materialView.each([&](auto entity, XJMaterialAssetRefComponent& materialRef)
        {
            for (const auto& material : materialRef.Materials)
            {
                if (material.Handle != assetHandle)
                    continue;

                XJEntity* xjEntity = scene.XJGetEntities(entity);
                if (xjEntity)
                    result.push_back(static_cast<XJEditorEntityId>(xjEntity->XJGetUUID()));

                break;
            }
        });

        auto sceneView = registry.view<XJSceneAssetRefComponent>();
        sceneView.each([&](auto entity, XJSceneAssetRefComponent& sceneRef)
        {
            if (sceneRef.SourceScene.Handle != assetHandle)
                return;

            XJEntity* xjEntity = scene.XJGetEntities(entity);
            if (!xjEntity)
                return;

            result.push_back(static_cast<XJEditorEntityId>(xjEntity->XJGetUUID()));
        });

        return result;
    }

    XJAssetHandle XJEditorSceneService::GetMeshAssetFromEntity(XJScene& scene, XJEditorEntityId entityId)
    {
        XJEntity* entity = FindEntityById(scene, entityId);
        if (!entity || !entity->HasComponent<XJMeshAssetRefComponent>())
            return 0;

        const auto& meshRef = entity->GetComponent<XJMeshAssetRefComponent>();
        return meshRef.Mesh.IsValid() ? meshRef.Mesh.Handle : 0;
    }

    void XJEditorSceneService::DeleteEntities(XJScene& scene, const std::vector<XJEditorEntityId>& entityIds)
    {
        std::unordered_set<XJEditorEntityId> uniqueIds(entityIds.begin(), entityIds.end());

        for (XJEditorEntityId id : uniqueIds)
        {
            XJEntity* entity = FindEntityById(scene, id);
            if (!entity || !entity->IsValid())
                continue;

            scene.DestroyEntity(entity);
        }
    }

    XJEditorSceneViewModel XJEditorSceneService::BuildSceneViewModel(XJScene& scene)
    {
        XJEditorSceneViewModel viewModel;

        XJNode* root = scene.XJGetRootNode();
        if (!root)
            return viewModel;

        for (XJNode* child : root->XJGetChildren())
        {
            if (XJEntity* entity = dynamic_cast<XJEntity*>(child))
                viewModel.RootEntities.push_back(BuildEntityViewRecursive(entity));
        }

        return viewModel;
    }

    XJEditorEntityDetailsView XJEditorSceneService::BuildEntityDetailsView(XJScene& scene, XJEditorEntityId entityId)
    {
        XJEditorEntityDetailsView details;

        XJEntity* entity = FindEntityById(scene, entityId);
        if (!entity || !entity->IsValid())
            return details;

        details.Valid = true;
        details.Id = entityId;
        details.Name = entity->XJGetName().empty() ? "XJUnnamed" : entity->XJGetName();

        if (entity->HasComponent<XJTransformComponent>())
        {
            const auto& transform = entity->GetComponent<XJTransformComponent>();
            details.Transform.Valid = true;
            details.Transform.Position = transform.position;
            details.Transform.Rotation = transform.rotation;
            details.Transform.Scale = transform.scale;
        }

        if (entity->HasComponent<XJCameraComponent>())
        {
            const auto& camera = entity->GetComponent<XJCameraComponent>();
            details.Camera.Valid = true;
            details.Camera.Fov = camera.XJGetFov();
            details.Camera.NearPlane = camera.XJGetNear();
            details.Camera.FarPlane = camera.XJGetFar();

            const char* modeNames[] = { "Orbit", "Free" };
            int modeIndex = static_cast<int>(camera.XJGetCameraMode());
            if (modeIndex >= 0 && modeIndex < 2)
                details.Camera.ModeName = modeNames[modeIndex];
            else
                details.Camera.ModeName = "Unknown";
        }

        if (entity->HasComponent<XJMeshAssetRefComponent>())
        {
            const auto& meshRef = entity->GetComponent<XJMeshAssetRefComponent>();
            details.Mesh.Valid = meshRef.Mesh.IsValid();
            details.Mesh.MeshAsset = meshRef.Mesh.Handle;
            details.Mesh.MeshUri = meshRef.Mesh.ToUri();
        }

        if (entity->HasComponent<XJSceneAssetRefComponent>())
        {
            const auto& sceneRef = entity->GetComponent<XJSceneAssetRefComponent>();
            details.SceneRef.Valid = true;
            details.SceneRef.SourceSceneUri = sceneRef.SourceScene.ToUri();
            details.SceneRef.SourceEntity = static_cast<uint64_t>(sceneRef.SourceEntity);
        }

        return details;
    }

    void XJEditorSceneService::RenameEntity(XJScene& scene, XJEditorEntityId entityId, const std::string& name)
    {
        XJEntity* entity = FindEntityById(scene, entityId);
        if (!entity || !entity->IsValid())
            return;

        entity->XJSetName(name);
    }

    void XJEditorSceneService::UpdateTransform(XJScene& scene, const XJEditorUpdateTransformRequest& request)
    {
        XJEntity* entity = FindEntityById(scene, request.EntityId);
        if (!entity || !entity->IsValid() || !entity->HasComponent<XJTransformComponent>())
            return;

        auto& transform = entity->GetComponent<XJTransformComponent>();
        transform.position = request.Position;
        transform.rotation = request.Rotation;
        transform.scale = request.Scale;
        transform.UpdateModelMatrix();
    }

    void XJEditorSceneService::UpdateCamera(XJScene& scene, const XJEditorUpdateCameraRequest& request)
    {
        XJEntity* entity = FindEntityById(scene, request.EntityId);
        if (!entity || !entity->IsValid() || !entity->HasComponent<XJCameraComponent>())
            return;

        auto& camera = entity->GetComponent<XJCameraComponent>();
        camera.XJSetFov(request.Fov);
        camera.XJSetNear(request.NearPlane);
        camera.XJSetFar(request.FarPlane);
    }

    XJEditorEntityId XJEditorSceneService::CreateEmptyEntity(XJScene& scene, const std::string& name, XJEditorEntityId parentId)
    {
        XJEntity* entity = scene.CreateEntity(name.empty() ? "Empty Entity" : name);
        if (!entity)
            return XJ_INVALID_EDITOR_ENTITY_ID;

        if (parentId != XJ_INVALID_EDITOR_ENTITY_ID)
        {
            XJEntity* parent = FindEntityById(scene, parentId);
            if (parent)
                parent->XJAddChild(entity);
        }

        return static_cast<XJEditorEntityId>(entity->XJGetUUID());
    }

    bool XJEditorSceneService::AddComponent(XJScene& scene, XJEditorEntityId entityId, XJEditorComponentType componentType)
    {
        XJEntity* entity = FindEntityById(scene, entityId);

        if(!entity || !entity->IsValid())
            return false;
        
        switch (componentType)
        {
            case XJEditorComponentType::Transform:
            {
                if (entity->HasComponent<XJTransformComponent>())
                    return false;

                auto& transform = entity->AddComponent<XJTransformComponent>();
                transform.position = glm::vec3(0.0f);
                transform.rotation = glm::vec3(0.0f);
                transform.scale = glm::vec3(1.0f);
                transform.UpdateModelMatrix();
                return true;
            }

            case XJEditorComponentType::Camera:
            {
                if (entity->HasComponent<XJCameraComponent>())
                    return false;

                if (!entity->HasComponent<XJTransformComponent>())
                {
                    auto& transform = entity->AddComponent<XJTransformComponent>();
                    transform.position = glm::vec3(0.0f);
                    transform.rotation = glm::vec3(0.0f);
                    transform.scale = glm::vec3(1.0f);
                    transform.UpdateModelMatrix();
                }

                auto& camera = entity->AddComponent<XJCameraComponent>();
                camera.XJSetFov(60.0f);
                camera.XJSetNear(0.1f);
                camera.XJSetFar(100.0f);
                return true;
            }

            default:
                return false;
        }
    }
}