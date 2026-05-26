#include "Asset/Instantiation/XJSceneInstantiator.h"

#include "Asset/Importer/XJModelImporter.h"
#include "Asset/XJAssetRegistry.h"
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"
#include "Render/Resource/XJMaterialFactory.h"
#include "Render/Resource/XJMeshFactory.h"

namespace XJ
{
    bool XJSceneInstantiator::Instantiate(const XJSceneAsset& asset, XJScene& outScene, XJSceneInstantiateContext* ctx)
    {
        XJSceneInstantiateContext localCtx;
        if (!ctx)
            ctx = &localCtx;

        ctx->EntityMap.clear();

        for (const auto& ed : asset.Entities)
        {
            auto* entity = CreateEntity(ed, outScene, *ctx);
            if (entity)
                ctx->EntityMap[ed.Id] = entity;
        }

        ApplyHierarchy(asset, *ctx);
        return true;
    }

    XJEntity* XJSceneInstantiator::CreateEntity(const XJSceneEntityData& data, XJScene& scene, XJSceneInstantiateContext& ctx)
    {
        auto* entity = scene.CreateEntityWithUUID(data.Id, data.Name);
        if (!entity)
            return nullptr;

        if (ctx.SourceScene.IsValid())
        {
            auto& source = entity->AddComponent<XJSceneAssetRefComponent>();
            source.SourceScene = ctx.SourceScene;
            source.SourceEntity = data.Id;
        }

        ApplyTransform(data, *entity);
        ApplyMeshRenderer(data, *entity, ctx);
        ApplyCamera(data, *entity);
        return entity;
    }

    void XJSceneInstantiator::ApplyTransform(const XJSceneEntityData& data, XJEntity& entity)
    {
        auto& t = entity.GetComponent<XJTransformComponent>();
        t.position = data.Transform.Position;
        t.rotation = data.Transform.Rotation;
        t.scale = data.Transform.Scale;
        t.UpdateModelMatrix();
    }

    void XJSceneInstantiator::ApplyMeshRenderer(const XJSceneEntityData& data, XJEntity& entity, XJSceneInstantiateContext& ctx)
    {
        if (data.MeshRenderer.Mesh.IsValid())
        {
            auto& meshRef = entity.AddComponent<XJMeshAssetRefComponent>();
            meshRef.Mesh = data.MeshRenderer.Mesh;
        }

        if (!data.MeshRenderer.Materials.empty())
        {
            auto& materialRef = entity.AddComponent<XJMaterialAssetRefComponent>();
            materialRef.Materials = data.MeshRenderer.Materials;
        }

        if (!data.MeshRenderer.Mesh.IsValid())
            return;

        XJAssetHandle meshHandle = data.MeshRenderer.Mesh.Handle;
        std::shared_ptr<XJMesh> gpuMesh;

        auto cacheIt = ctx.MeshCache.find(meshHandle);
        if (cacheIt != ctx.MeshCache.end())
        {
            gpuMesh = cacheIt->second;
        }
        else if (ctx.Registry && ctx.Registry->Contains(meshHandle))
        {
            auto meta = ctx.Registry->GetMeta(meshHandle);
            if (meta.has_value())
            {
                XJGltfImporter importer;
                if (importer.LoadMeshAsset(meta->SourcePath.string()))
                {
                    auto meshAsset = importer.ExtractMesh(0);
                    if (meshAsset && !meshAsset->mVertices.empty())
                        gpuMesh = XJMeshFactory::CreateFromAsset(*meshAsset);
                }
            }
            ctx.MeshCache[meshHandle] = gpuMesh;
        }

        if (!gpuMesh || !ctx.DefaultTexture || !ctx.DefaultSampler)
            return;

        auto& comp = entity.AddComponent<XJUnlitMaterialComponent>();
        auto mat = XJMaterialFactory::GetInstance()->CreateMaterial<XJUnlitMaterial>();
        mat->XJSetBaseColorA(glm::vec3(0.8f, 0.6f, 0.2f));
        mat->XJSetBaseColorB(glm::vec3(0.8f, 0.6f, 0.2f));
        mat->XJSetTextureView(UNLIT_MAT_BASE_COLOR_A, ctx.DefaultTexture, ctx.DefaultSampler);
        mat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_A, false);
        mat->XJSetTextureView(UNLIT_MAT_BASE_COLOR_B, ctx.DefaultTexture, ctx.DefaultSampler);
        mat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_B, false);
        comp.AddMesh(gpuMesh.get(), mat.get());
    }

    void XJSceneInstantiator::ApplyCamera(const XJSceneEntityData& data, XJEntity& entity)
    {
        if (!data.Camera.Enabled)
            return;

        auto& cam = entity.AddComponent<XJCameraComponent>();
        cam.XJSetFov(data.Camera.Fov);
        cam.XJSetNear(data.Camera.NearClip);
        cam.XJSetFar(data.Camera.FarClip);
    }

    void XJSceneInstantiator::ApplyHierarchy(const XJSceneAsset& asset, XJSceneInstantiateContext& ctx)
    {
        for (const auto& ed : asset.Entities)
        {
            if (ed.Parent == 0)
                continue;

            auto parentIt = ctx.EntityMap.find(ed.Parent);
            auto childIt = ctx.EntityMap.find(ed.Id);
            if (parentIt == ctx.EntityMap.end() || childIt == ctx.EntityMap.end())
                continue;

            if (parentIt->second && childIt->second)
                parentIt->second->XJAddChild(childIt->second);
        }
    }
}
