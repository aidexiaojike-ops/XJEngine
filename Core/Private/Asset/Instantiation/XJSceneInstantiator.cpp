#include "Asset/Instantiation/XJSceneInstantiator.h"
#include "Asset/Importer/XJMaterialImporter.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/Loader/XJMeshAssetLoader.h"

#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"
#include "Render/Resource/XJMaterialFactory.h"

namespace XJ
{

    namespace
    {
        std::shared_ptr<XJUnlitMaterial> CreateMaterialForSlot(const std::vector<XJAssetRef>& materials, uint32_t slotIndex, XJSceneInstantiateContext& ctx)
        {
            if (slotIndex < materials.size())
            {
                const XJAssetRef& materialRef = materials[slotIndex];

                if (materialRef.IsValid() && ctx.Registry)
                {
                    auto meta = ctx.Registry->GetMeta(materialRef.Handle);
                    if (meta && meta->Type == XJAssetType::Material)
                    {
                        auto materialAsset = XJMaterialImporter::ImportMaterial(meta->SourcePath.string());
                        if (materialAsset)
                        {
                            materialAsset->mHandle = meta->Handle;
                            materialAsset->mName = meta->Name;
                            materialAsset->mPath = meta->SourcePath;

                            return XJMaterialFactory::GetInstance()->CreateFromAsset(*materialAsset, ctx.DefaultTexture, ctx.DefaultSampler);
                        }
                    }
                }
            }

            return XJMaterialFactory::GetInstance()->CreateDefaultMaterial(ctx.DefaultTexture, ctx.DefaultSampler);
        }
    }

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
                ctx->EntityMap[ed.UUID] = entity;
        }

        ApplyHierarchy(asset, *ctx);
        return true;
    }

    XJEntity* XJSceneInstantiator::CreateEntity(const XJSceneEntityData& data, XJScene& scene, XJSceneInstantiateContext& ctx)
    {
        auto* entity = scene.CreateEntityWithUUID(data.UUID, data.Name);
        if (!entity)
            return nullptr;

        if (ctx.SourceScene.IsValid())
        {
            auto& source = entity->AddComponent<XJSceneAssetRefComponent>();
            source.SourceScene = ctx.SourceScene;
            source.SourceEntity = data.UUID;
        }

        if(data.HasTransform)
            ApplyTransform(data, *entity);

        if(data.HasMeshRenderer)
            ApplyMeshRenderer(data, *entity, ctx);

        if(data.HasCamera)    
            ApplyCamera(data, *entity);
        return entity;
    }

    //添加组件

    void XJSceneInstantiator::ApplyTransform(const XJSceneEntityData& data, XJEntity& entity)
    {
        XJTransformComponent* transform = nullptr;

        if (entity.HasComponent<XJTransformComponent>())
            transform = &entity.GetComponent<XJTransformComponent>();
        else
            transform = &entity.AddComponent<XJTransformComponent>();

        auto& t = *transform;

        if (data.Transform.UUID != 0)
            t.XJSetUUID(data.Transform.UUID);

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
            
            if (data.MeshRenderer.UUID != 0)
                meshRef.XJSetUUID(data.MeshRenderer.UUID);

            meshRef.Mesh = data.MeshRenderer.Mesh;
        }

        if (!data.MeshRenderer.Materials.empty())
        {
            auto& materialRef = entity.AddComponent<XJMaterialAssetRefComponent>();
            materialRef.Materials = data.MeshRenderer.Materials;
        }

        if (!data.MeshRenderer.Mesh.IsValid())
            return;

        XJMeshAssetLoadContext loadContext;//加载网格资源需要的上下文，包含注册表和缓存等
        loadContext.Registry = ctx.Registry;
        loadContext.MeshCache = &ctx.MeshCache;
        std::shared_ptr<XJMesh> gpuMesh = XJMeshAssetLoader::LoadMesh(data.MeshRenderer.Mesh.Handle, loadContext);

        if (!gpuMesh || !ctx.DefaultTexture || !ctx.DefaultSampler)
            return;

        auto& comp = entity.AddComponent<XJUnlitMaterialComponent>();
        auto mat = CreateMaterialForSlot(data.MeshRenderer.Materials, 0, ctx);
        if (!mat)
            return;
            
        comp.AddMesh(gpuMesh.get(), mat.get());
    }

    void XJSceneInstantiator::ApplyCamera(const XJSceneEntityData& data, XJEntity& entity)
    {
        if (!data.Camera.Enabled)
            return;

        auto& cam = entity.AddComponent<XJCameraComponent>();
        if (data.Camera.UUID != 0)
            cam.XJSetUUID(data.Camera.UUID);

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
            auto childIt = ctx.EntityMap.find(ed.UUID);
            if (parentIt == ctx.EntityMap.end() || childIt == ctx.EntityMap.end())
                continue;

            if (parentIt->second && childIt->second)
                parentIt->second->XJAddChild(childIt->second);
        }
    }

    XJEntity* XJSceneInstantiator::FindInstantiatedEntity(const XJSceneInstantiateContext& ctx, XJUUID id) 
    {
        auto it = ctx.EntityMap.find(id);
        return (it != ctx.EntityMap.end()) ? it->second : nullptr;
    }
}
