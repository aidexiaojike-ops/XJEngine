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

#include <spdlog/spdlog.h>

#include <unordered_map>

namespace XJ
{

    namespace
    {
        enum class HierarchyVisitState
        {
            Visiting,
            Visited
        };

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

        bool HasHierarchyCycleFrom(
            XJUUID uuid,
            const std::unordered_map<XJUUID, XJUUID>& parentByChild,
            std::unordered_map<XJUUID, HierarchyVisitState>& visitStates)
        {
            if (uuid == 0)
                return false;

            auto stateIt = visitStates.find(uuid);
            if (stateIt != visitStates.end())
                return stateIt->second == HierarchyVisitState::Visiting;

            visitStates[uuid] = HierarchyVisitState::Visiting;

            auto parentIt = parentByChild.find(uuid);
            if (parentIt != parentByChild.end())
            {
                if (HasHierarchyCycleFrom(parentIt->second, parentByChild, visitStates))
                    return true;
            }

            visitStates[uuid] = HierarchyVisitState::Visited;
            return false;
        }

        bool HasHierarchyCycle(const XJSceneAsset& asset)
        {
            std::unordered_map<XJUUID, XJUUID> parentByChild;

            for (const auto& ed : asset.Entities)
            {
                if (ed.UUID != 0 && ed.Parent != 0)
                    parentByChild[ed.UUID] = ed.Parent;
            }

            std::unordered_map<XJUUID, HierarchyVisitState> visitStates;
            for (const auto& [child, parent] : parentByChild)
            {
                if (HasHierarchyCycleFrom(child, parentByChild, visitStates))
                    return true;
            }

            return false;
        }
    }

    bool XJSceneInstantiator::Instantiate(const XJSceneAsset& asset, XJScene& outScene, XJSceneInstantiateContext* ctx)
    {
        XJSceneInstantiateContext localCtx;
        if (!ctx)
            ctx = &localCtx;

        // Instantiate 是“用资产重建目标 scene”，不是追加。
        // 追加语义以后应单独提供 AppendInstantiate，避免重复 UUID 生成重复实体。
        outScene.DestroyAllEntity();
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

        if (data.HasLight)
        {
            // 资产层已经保留 light 数据；runtime LightComponent 添加后，在这里接 ApplyLight。
            spdlog::debug("Scene instantiate skipped light component: runtime light component is not implemented yet.");
        }

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
        comp.ClearMeshes();

        const uint32_t slotCount = data.MeshRenderer.Materials.empty()
            ? 1u
            : static_cast<uint32_t>(data.MeshRenderer.Materials.size());

        // 当前 loader 返回的是合并后的 XJMesh，尚未暴露 glTF primitive/submesh 数量。
        // 先按序列化的 material slot 数量恢复，避免 materials[1..n] 被静默忽略。
        for (uint32_t slotIndex = 0; slotIndex < slotCount; ++slotIndex)
        {
            auto mat = CreateMaterialForSlot(data.MeshRenderer.Materials, slotIndex, ctx);
            if (mat)
                comp.AddMesh(gpuMesh, mat);
        }
    }

    void XJSceneInstantiator::ApplyCamera(const XJSceneEntityData& data, XJEntity& entity)
    {
        auto& cam = entity.AddComponent<XJCameraComponent>();
        if (data.Camera.UUID != 0)
            cam.XJSetUUID(data.Camera.UUID);

        cam.XJSetEnabled(data.Camera.Enabled);
        cam.XJSetFov(data.Camera.Fov);
        cam.XJSetNear(data.Camera.NearClip);
        cam.XJSetFar(data.Camera.FarClip);
    }

    void XJSceneInstantiator::ApplyHierarchy(const XJSceneAsset& asset, XJSceneInstantiateContext& ctx)
    {
        if (HasHierarchyCycle(asset))
        {
            // 坏资产中的 A->B->A 会形成环。即使 XJNode 有运行时防护，
            // 实例化阶段也应直接拒绝恢复这批层级数据，避免留下半正确树结构。
            spdlog::error("Scene instantiate skipped hierarchy restore: cycle detected in scene asset.");
            return;
        }

        for (const auto& ed : asset.Entities)
        {
            if (ed.Parent == 0)
                continue;

            if (ed.Parent == ed.UUID)
            {
                spdlog::warn("Scene instantiate skipped self-parent entity uuid={}", static_cast<uint64_t>(ed.UUID));
                continue;
            }

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
