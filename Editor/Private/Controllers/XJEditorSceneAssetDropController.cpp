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
#include <cmath>
#include <spdlog/spdlog.h>


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

        XJEntity* entity = scene.CreateEntity(metaOpt->Name);
        if (!entity)
            return false;

        auto& transform = entity->GetComponent<XJTransformComponent>();
        transform.position = CalculateSpawnPositionFromDropRay(scene, payload);
        transform.UpdateModelMatrix();// 更新模型矩阵以应用位置

        auto& meshRef = entity->AddComponent<XJMeshAssetRefComponent>();
        meshRef.Mesh = { payload.Handle, XJAssetType::Mesh };

        XJMeshAssetLoadContext loadContext;
        loadContext.Registry = &assetRegistry;
        loadContext.MeshCache = &instantiateContext.MeshCache;

        std::shared_ptr<XJMesh> gpuMesh = XJMeshAssetLoader::LoadMesh(payload.Handle, loadContext);

        if (gpuMesh)
        {
            auto& comp = entity->AddComponent<XJUnlitMaterialComponent>();
            auto material = XJMaterialFactory::GetInstance()->CreateDefaultMaterial(
                defaultTexture,
                defaultSampler);

            if (material)
                comp.AddMesh(gpuMesh.get(), material.get());
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

         if (RaycastSceneWithinDistance(scene, payload.RayOrigin, payload.RayDirection, 5.0f, spawnPosition))
         {
             return spawnPosition;
         }

         return payload.RayOrigin + payload.RayDirection * 5.0f;
    }

        //射线距离
    bool XJEditorSceneAssetDropController::RaycastSceneWithinDistance(XJScene& scene, const glm::vec3& rayOrigin, const glm::vec3& rayDirection, float maxDistance, glm::vec3& outSpawnPosition)
    {
        float closestT = maxDistance;
        bool hit = false;

        auto& registry = scene.XJGetEcsRegistry();
        auto view = registry.view<XJTransformComponent, XJMeshAssetRefComponent>();

        view.each([&](auto entity, XJTransformComponent& transform, XJMeshAssetRefComponent& meshRef)
        {
            float radius = std::max(transform.scale.x, std::max(transform.scale.y, transform.scale.z));
            radius = std::max(radius, 0.5f);

            float t = 0.0f;
            if (!IntersectRaySphere(rayOrigin, rayDirection, transform.position, radius, maxDistance, t))
                return;

            if (t < closestT)
            {
                closestT = t;
                hit = true;

                outSpawnPosition = transform.position;
                outSpawnPosition.y += radius + 0.05f;
            }
        });

        return hit;
    }

        //临时的碰撞球
    bool XJEditorSceneAssetDropController::IntersectRaySphere(const glm::vec3& rayOrigin,const glm::vec3& rayDir,const glm::vec3& center,float radius,float maxDistance,float& outT)
    {
        glm::vec3 oc = rayOrigin - center;

        float b = glm::dot(oc, rayDir);
        float c = glm::dot(oc, oc) - radius * radius;
        float h = b * b - c;

        if (h < 0.0f)
            return false;

        h = std::sqrt(h);

        float t = -b - h;
        if (t < 0.0f)
            t = -b + h;

        if (t < 0.0f || t > maxDistance)
            return false;

        outT = t;
        return true;
    }

}