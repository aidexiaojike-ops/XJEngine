#include "Controllers/XJEditorSceneAssetDropController.h"

#include "Asset/Loader/XJMeshAssetLoader.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/Instantiation/XJSceneInstantiator.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/XJTransformComponent.h"
#include "Render/Resource/XJMaterialFactory.h"
#include "UI/XJEditorDragPayload.h"
#include "UI/XJEditorUIState.h"

#include <algorithm>
#include <spdlog/spdlog.h>

#include "Geometry/XJRayIntersection.h"
#include "Render/Resource/XJMesh.h"


namespace XJ
{
    bool XJEditorSceneAssetDropController::CreateEntityFromDroppedAsset(
        XJScene& scene,
        const XJAssetDragPayload& payload,
        XJAssetRegistry& assetRegistry,
        XJSceneInstantiateContext& instantiateContext,
        XJEditorUIState& uiState,
        const std::shared_ptr<XJTexture>& defaultTexture,
        const std::shared_ptr<XJSampler>& defaultSampler)// 处理从资源浏览器拖放到场景预览的资产，创建对应的实体
    {
        if (payload.Type != XJAssetType::Mesh || payload.Handle == 0)
            return false;

        auto metaOpt = assetRegistry.GetMeta(payload.Handle);
        if (!metaOpt)
        {
            spdlog::error("Asset meta not found for handle: 0x{:016X}", payload.Handle);
                return false;
        }

        XJEntity* entity = scene.CreateEntityWithTransform(metaOpt->Name);//创建实例添加tansform组件
        if (!entity)
            return false;

        auto& transform = entity->GetComponent<XJTransformComponent>();
        transform.position = CalculateSpawnPositionFromDropRay(scene, payload);
        transform.UpdateModelMatrix();// 更新模型矩阵以应用位置
  
        if (instantiateContext.SourceScene.IsValid())
        {
            auto& sceneRef = entity->AddComponent<XJSceneAssetRefComponent>();
            sceneRef.SourceScene = instantiateContext.SourceScene;
            sceneRef.SourceEntity = entity->XJGetUUID();
        }

        auto& meshRef = entity->AddComponent<XJMeshAssetRefComponent>();
        meshRef.Mesh = { payload.Handle, XJAssetType::Mesh };

        XJMeshAssetLoadContext loadContext;
        loadContext.Registry = &assetRegistry;
        loadContext.MeshCache = &instantiateContext.MeshCache;

        std::shared_ptr<XJMesh> gpuMesh = XJMeshAssetLoader::LoadMesh(payload.Handle, loadContext);

        if (!gpuMesh)
        {
            spdlog::error("Dropped mesh failed to load; entity creation was rolled back: handle=0x{:016X}", payload.Handle);
            scene.DestroyEntity(entity);
            return false;
        }

        auto material = XJMaterialFactory::GetInstance()->GetOrCreateDefaultMaterial(
            defaultTexture,
            defaultSampler);
        if (!material)
        {
            spdlog::error("Dropped mesh default material creation failed; entity creation was rolled back: handle=0x{:016X}", payload.Handle);
            scene.DestroyEntity(entity);
            return false;
        }

        const uint32_t submeshCount = gpuMesh->GetSubmeshCount();
        if(submeshCount == 0)
        {
            spdlog::error("Dropped mesh has no submeshes; " "entity creation was rolled back.");

            scene.DestroyEntity(entity);
            return false;
        }
        const uint32_t materialSlotCount = gpuMesh->GetMaterialSlotCount();

        if (materialSlotCount == 0)
        {
            spdlog::error(
                "Dropped mesh has no material slots.");
            
            scene.DestroyEntity(entity);
            return false;
        }

        auto& comp = entity->AddComponent<XJUnlitMaterialComponent>();
        auto& materialRefs = entity->AddComponent<XJMaterialAssetRefComponent>();
        // 新拖入实体使用默认材质，但资产引用槽必须与 Mesh 的 MaterialSlot 对齐。
        materialRefs.Materials.resize(materialSlotCount);

        for(uint32_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
        {
            // 拖入新 Mesh 时所有 submesh 暂时使用同一个默认材质
            comp.AddMesh(gpuMesh, material, submeshIndex);
        }

        uiState.Selection.SelectedEntity = static_cast<XJEditorEntityId>(entity->XJGetUUID());
        uiState.Selection.SelectedAsset = 0;
        uiState.Selection.HighlightedEntities.clear();

        return true;
    }

    glm::vec3 XJEditorSceneAssetDropController::CalculateSpawnPositionFromDropRay(XJScene& scene, const XJAssetDragPayload& payload)//拖拽资产 生成位置
    {
        if (!payload.HasViewportRay)
             return glm::vec3(0.0f);

         glm::vec3 spawnPosition{0.0f};

         const float rayMaxDistance = std::max(payload.RayMaxDistance, 0.1f);

         if (RaycastSceneWithinDistance(scene, payload.RayOrigin, payload.RayDirection, rayMaxDistance, spawnPosition))
         {
             return spawnPosition;
         }

         // 未命中时仍放在摄像机附近，不直接放到 Far Plane。
         const float fallbackDistance = std::min(5.0f, rayMaxDistance);
         return payload.RayOrigin + glm::normalize(payload.RayDirection) * fallbackDistance;
    }

        //射线距离
    bool XJEditorSceneAssetDropController::RaycastSceneWithinDistance(XJScene& scene, const glm::vec3& rayOrigin, const glm::vec3& rayDirection, float maxDistance, glm::vec3& outSpawnPosition)
    {
        if (maxDistance <= 0.0f)
            return false;

        float closestDistance = maxDistance;
        bool foundHit = false;

        const auto& registry = scene.XJGetEcsRegistry();

        // MeshAssetRef 只有资产 handle，没有 GPU Mesh/Bounds。
        // Runtime material component 才保存真正的 XJMesh。
        auto view = registry.view<XJTransformComponent, XJUnlitMaterialComponent>();

        view.each([&](auto entity, const XJTransformComponent& transform, const XJUnlitMaterialComponent& renderComponent)
        {
            (void)entity;

            std::shared_ptr<XJMesh> mesh;

            // 同一个 Mesh 的多个 submesh 会产生多个 draw slot，
            // 这里只需要取得第一个有效 Mesh 并检测整体 Bounds。
            for (const auto& slot : renderComponent.XJGetSlots())
            {
                if (slot.Mesh)
                {
                    mesh = slot.Mesh;
                    break;
                }
            }

            if (!mesh || !mesh->GetBounds().IsValid())
            {
                return;
            }

            const XJBoundingBox worldBounds = mesh->GetBounds().Transformed( transform.GetModelMatrix());

            if (!worldBounds.IsValid())
                return;

            XJRayAABBHit hit;

            if (!XJIntersectRayAABB(
                    rayOrigin,
                    rayDirection,
                    worldBounds,
                    closestDistance,
                    hit))
            {
                return;
            }

            if (!hit.Hit || hit.Distance >= closestDistance)
            {
                return;
            }

            closestDistance = hit.Distance;
            foundHit = true;

            // 稍微沿表面法线推出，避免新物体原点正好落入旧物体内部。
            outSpawnPosition = hit.Position + hit.Normal * 0.05f;
        });

        return foundHit;
    }

}
