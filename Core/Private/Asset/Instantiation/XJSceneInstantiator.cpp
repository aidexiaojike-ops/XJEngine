#include "Asset/Instantiation/XJSceneInstantiator.h"
#include "Asset/Importer/XJMaterialImporter.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/Loader/XJMeshAssetLoader.h"
#include "Asset/Loader/XJScriptAssetLoader.h"
#include "Asset/XJScriptAsset.h"

#include "ECS/Component/Material/XJSurfaceMaterialComponent.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/Component/XJLightComponent.h"
#include "ECS/Component/XJScriptComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"
#include "Render/Resource/XJMaterialFactory.h"
#include "Render/Resource/XJMesh.h"

#include <spdlog/spdlog.h>

#include <exception>
#include <unordered_map>
#include <unordered_set>

namespace XJ
{

    namespace
    {
        enum class HierarchyVisitState
        {
            Visiting,
            Visited
        };

        std::shared_ptr<XJSurfaceMaterial> CreateMaterialForSlot(const std::vector<XJAssetRef>& materials, uint32_t slotIndex, XJSceneInstantiateContext& ctx)
        {
            XJMaterialFactory* factory = XJMaterialFactory::GetInstance();

             // 槽位不存在或引用为空，才允许使用默认材质。
            if(slotIndex >= materials.size() || !materials[slotIndex].IsValid())
            {
                std::shared_ptr<XJSurfaceMaterial> defaultMaterial;
                if(ctx.MaterialPolicy == XJSceneMaterialPolicy::IsolatedScene)
                {
                    if(!ctx.SceneDefaultMaterial)
                    {
                        ctx.SceneDefaultMaterial = factory->CreateDefaultMaterial(ctx.DefaultTexture, ctx.DefaultSampler);
                    }
                    defaultMaterial = ctx.SceneDefaultMaterial;
                }
                else
                {
                    defaultMaterial = factory->GetOrCreateDefaultMaterial(ctx.DefaultTexture, ctx.DefaultSampler);
                }

                return defaultMaterial && defaultMaterial->HasRuntimeParameterBlock()
                    ? defaultMaterial
                    : nullptr;
            }

            const XJAssetRef& materialRef = materials[slotIndex];
            // 本次 Scene 内先复用已经准备好的实例。
            auto cached = ctx.SceneMaterialCache.find(materialRef.Handle);

            if(cached != ctx.SceneMaterialCache.end())
                return cached->second;
            
            if(!ctx.Registry)
                return nullptr;
            
            auto meta = ctx.Registry->GetMeta(materialRef.Handle);

            if(!meta || meta->Type != XJAssetType::Material)
            {
                spdlog::error(
                    "Material load failed: invalid handle={}.",
                    materialRef.Handle);
                return nullptr;
            }

            auto materialAsset = XJMaterialImporter::ImportMaterial(meta->SourcePath.string());

            if (!materialAsset)
            {
                spdlog::error(
                    "Material load failed: '{}'.",
                    meta->SourcePath.string());
                return nullptr;
            }

            materialAsset->mHandle = meta->Handle;
            materialAsset->mName = meta->Name;
            materialAsset->mPath = meta->SourcePath;



            std::shared_ptr<XJSurfaceMaterial> material;

            if (ctx.MaterialPolicy == XJSceneMaterialPolicy::IsolatedScene)
            {
                material = factory->CreateFromAsset(
                    *materialAsset,
                    ctx.DefaultTexture,
                    ctx.DefaultSampler);
            }
            else
            {
                material = factory->GetOrCreateFromAsset(
                    *materialAsset,
                    ctx.DefaultTexture,
                    ctx.DefaultSampler);
            }
        
            if (!material || !material->HasRuntimeParameterBlock())
            {
                spdlog::error(
                    "Material runtime creation failed: '{}'.",
                    meta->SourcePath.string());
                return nullptr;
            }
        
            ctx.SceneMaterialCache[materialRef.Handle] = material;
            
            return material;
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

        XJScriptValueType ScriptValueType(const XJScriptValue& value)
        {
            if (std::holds_alternative<bool>(value)) return XJScriptValueType::Bool;
            if (std::holds_alternative<int64_t>(value)) return XJScriptValueType::Int;
            if (std::holds_alternative<double>(value)) return XJScriptValueType::Float;
            return XJScriptValueType::String;
        }
    }

    bool XJSceneInstantiator::ValidateAsset(const XJSceneAsset& asset, XJSceneInstantiateContext& ctx)
    {
        std::unordered_set<XJUUID> entityIds;
        std::unordered_set<XJUUID> scriptComponentIds;
        entityIds.reserve(asset.Entities.size());

        for(const auto& entityData : asset.Entities)
        {
            if(entityData.UUID == 0)
            {
                spdlog::error("Scene validation failed: entity '{}' has invalid UUID.", entityData.Name);
                return false;
            }

            if(!entityIds.insert(entityData.UUID).second)
            {
                spdlog::error("Scene validation failed: duplicate entity UUID={}.", static_cast<uint64_t>(entityData.UUID));
                return false;
            }

            if (entityData.HasScript)
            {
                if (!entityData.Script.Valid || entityData.Script.UUID == 0 ||
                    !scriptComponentIds.insert(entityData.Script.UUID).second)
                {
                    spdlog::error("Scene validation failed: invalid or duplicate ScriptComponent UUID.");
                    return false;
                }
            }
        }
        for (const auto& entityData : asset.Entities)
        {
            if (entityData.Parent == 0)
                continue;
        
            if (entityData.Parent == entityData.UUID)
            {
                spdlog::error(
                    "Scene validation failed: entity UUID={} references itself as parent.",
                    static_cast<uint64_t>(entityData.UUID));
                return false;
            }
        
            if (!entityIds.contains(entityData.Parent))
            {
                spdlog::error(
                    "Scene validation failed: entity UUID={} references missing parent UUID={}.",
                    static_cast<uint64_t>(entityData.UUID),
                    static_cast<uint64_t>(entityData.Parent));
                return false;
            }

        }
        if (HasHierarchyCycle(asset))
        {
            spdlog::error(
                "Scene validation failed: "
                "hierarchy contains a cycle.");
            return false;
        }

        XJMeshAssetLoadContext meshLoadContext;
        meshLoadContext.Registry = ctx.Registry;
        meshLoadContext.MeshCache = &ctx.MeshCache;

        for (const auto& entityData : asset.Entities)
        {
            if (!entityData.HasMeshRenderer)
                continue;

            if (!entityData.MeshRenderer.Mesh.IsValid())
            {
                spdlog::error(
                    "Scene validation failed: mesh entity UUID={} has no valid mesh reference.",
                    static_cast<uint64_t>(entityData.UUID));
                return false;
            }

            if (!ctx.Registry ||
                !ctx.DefaultTexture ||
                !ctx.DefaultSampler)
            {
                spdlog::error(
                    "Scene validation failed: mesh instantiation context is incomplete.");
                return false;
            }

            auto meshMeta = ctx.Registry->GetMeta(
                entityData.MeshRenderer.Mesh.Handle);

            if (!meshMeta ||
                meshMeta->Type != XJAssetType::Mesh)
            {
                spdlog::error(
                    "Scene validation failed: invalid mesh handle={}.",
                    entityData.MeshRenderer.Mesh.Handle);
                return false;
            }

            auto mesh = XJMeshAssetLoader::LoadMesh(
                entityData.MeshRenderer.Mesh.Handle,
                meshLoadContext);

            if (!mesh ||
                !mesh->IsValid() ||
                mesh->GetSubmeshCount() == 0 ||
                mesh->GetMaterialSlotCount() == 0)
            {
                spdlog::error(
                    "Scene validation failed: mesh handle={} could not produce valid submeshes.",
                    entityData.MeshRenderer.Mesh.Handle);
                return false;
            }

            for (uint32_t slotIndex = 0;
                 slotIndex < mesh->GetMaterialSlotCount();
                 ++slotIndex)
            {
                // 空槽会创建默认材质；明确引用损坏则返回 nullptr。
                auto material = CreateMaterialForSlot(
                    entityData.MeshRenderer.Materials,
                    slotIndex,
                    ctx);

                if (!material)
                {
                    spdlog::error(
                        "Scene validation failed: entity UUID={}, material slot={}.",
                        static_cast<uint64_t>(entityData.UUID),
                        slotIndex);
                    return false;
                }
            }
        }

        if (ctx.RequireCompiledScripts)
        {
            if (!ctx.Registry)
            {
                spdlog::error("Scene validation failed: script validation requires an asset registry.");
                return false;
            }

            XJScriptAssetLoadContext scriptContext{ctx.Registry, &ctx.ScriptCache};
            for (const auto& entityData : asset.Entities)
            {
                if (!entityData.HasScript)
                    continue;

                for (const auto& slot : entityData.Script.Slots)
                {
                    auto script = XJScriptAssetLoader::LoadScript(slot.Script.Handle, scriptContext);
                    if (!script || !script->IsCompiled())
                    {
                        spdlog::error("Scene validation failed: script handle={} did not compile.",
                                      slot.Script.Handle);
                        return false;
                    }

                    std::unordered_map<uint64_t, const XJScriptBytecodeField*> publicFields;
                    for (const auto& field : script->Module->Fields)
                    {
                        if (field.Access == XJScriptAccess::Public && field.StableId != 0)
                            publicFields[field.StableId] = &field;
                    }

                    for (const auto& [fieldId, value] : slot.FieldOverrides)
                    {
                        const auto field = publicFields.find(fieldId);
                        if (field == publicFields.end() ||
                            ScriptValueType(value) != field->second->Type)
                        {
                            spdlog::error(
                                "Scene validation failed: script handle={} has invalid override fieldId={}.",
                                slot.Script.Handle, fieldId);
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }

    bool XJSceneInstantiator::Instantiate(const XJSceneAsset& asset, XJScene& outScene, XJSceneInstantiateContext* ctx)
    {
        XJSceneInstantiateContext localCtx;
        if (!ctx)
            ctx = &localCtx;

        ctx->TargetSceneModified = false;

        // 所有资源先写入临时 Context。验证失败时不会污染调用方缓存和 EntityMap。
        XJSceneInstantiateContext preparedContext = *ctx;
        preparedContext.EntityMap.clear();
        preparedContext.SceneMaterialCache.clear();
        preparedContext.SceneDefaultMaterial.reset();

        // 必须在清空目标 Scene 之前验证并准备 Mesh/Material。
        if (!ValidateAsset(asset, preparedContext))
            return false;

        // Instantiate 是“用资产重建目标 scene”，不是追加。
        // 追加语义以后应单独提供 AppendInstantiate，避免重复 UUID 生成重复实体。
        preparedContext.TargetSceneModified = true;
        ctx->TargetSceneModified = true;
        outScene.DestroyAllEntity();

        for (const auto& entityData : asset.Entities)
        {
            auto* entity = CreateEntity(entityData, outScene, preparedContext);

            if (!entity)
            {
                spdlog::error(
                    "Scene instantiate failed: entity UUID={} could not be created.",
                    static_cast<uint64_t>(
                        entityData.UUID));

                outScene.DestroyAllEntity();
                preparedContext.EntityMap.clear();
                return false;
            }

            preparedContext.EntityMap[entityData.UUID] = entity;
        }

        if (!ApplyHierarchy(asset, preparedContext))
        {
            spdlog::error(
                "Scene instantiate failed: hierarchy could not be restored.");
            
            outScene.DestroyAllEntity();
            preparedContext.EntityMap.clear();
            return false;
        }

        // EntityMap 只用于本次层级恢复，不能在 Context 中长期保存实体裸指针。
        preparedContext.EntityMap.clear();
        // Scene 和全部资源都成功后，才提交 Context。
        *ctx = std::move(preparedContext);
        return true;
    }

    XJEntity* XJSceneInstantiator::CreateEntity(const XJSceneEntityData& data, XJScene& scene, XJSceneInstantiateContext& ctx)
    {
        auto* entity = scene.CreateEntityWithUUID(data.UUID, data.Name);
        if (!entity)
            return nullptr;

        try
        {
            if (ctx.SourceScene.IsValid())
            {
                auto& source =
                    entity->AddComponent<
                        XJSceneAssetRefComponent>();
            
                source.SourceScene =
                    ctx.SourceScene;
                source.SourceEntity =
                    data.UUID;
            }
        
            if (data.HasTransform)
                ApplyTransform(data, *entity);
        
            if (data.HasMeshRenderer &&
                !ApplyMeshRenderer(
                    data,
                    *entity,
                    ctx))
            {
                scene.DestroyEntity(entity);
                return nullptr;
            }
        
            if (data.HasCamera)
                ApplyCamera(data, *entity);
        
            if (data.HasLight)
                ApplyLight(data, *entity);

            if (data.HasScript && !ApplyScript(data, *entity))
            {
                scene.DestroyEntity(entity);
                return nullptr;
            }
        }
        catch (const std::exception& exception)
        {
            spdlog::error(
                "Scene entity creation failed: UUID={}, error={}",
                static_cast<uint64_t>(data.UUID),
                exception.what());
            
            scene.DestroyEntity(entity);
            return nullptr;
        }

        return entity;
    }
    //添加代码组件
    bool XJSceneInstantiator::ApplyScript(const XJSceneEntityData& data, XJEntity& entity)
    {
        if (!data.Script.Valid)
            return false;

        auto& component = entity.AddComponent<XJScriptComponent>();
    
        if (data.Script.UUID != 0)
            component.XJSetUUID(
                data.Script.UUID);
            
        for (const auto& source :
             data.Script.Slots)
        {
            XJScriptSlot* slot =
                component.AddSlotWithId(
                    source.SlotId,
                    source.Script,
                    source.Enabled);
                
            if (!slot)
                return false;
                
            for (const auto& [
                     fieldId,
                     value] :
                 source.FieldOverrides)
            {
                if (!component.SetFieldOverride(
                        source.SlotId,
                        fieldId,
                        value))
                {
                    return false;
                }
            }
        }
    
        return true;
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

    bool XJSceneInstantiator::ApplyMeshRenderer(const XJSceneEntityData& data, XJEntity& entity, XJSceneInstantiateContext& ctx)
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
            return false;

        XJMeshAssetLoadContext loadContext;//加载网格资源需要的上下文，包含注册表和缓存等
        loadContext.Registry = ctx.Registry;
        loadContext.MeshCache = &ctx.MeshCache;
        std::shared_ptr<XJMesh> gpuMesh = XJMeshAssetLoader::LoadMesh(data.MeshRenderer.Mesh.Handle, loadContext);

        if (!gpuMesh || !ctx.DefaultTexture || !ctx.DefaultSampler)
            return false;

        auto& comp = entity.AddComponent<XJSurfaceMaterialComponent>();
        comp.ClearMeshes();

        const uint32_t submeshCount = gpuMesh->GetSubmeshCount();
        if(submeshCount == 0)//如果没材质通道 移除material 组件
        {
            spdlog::error(
                "Scene instantiate skipped mesh: "
                "GPU mesh contains no submeshes.");
                    
            entity.RemoveComponent<XJSurfaceMaterialComponent>();
                    
            return false;
        }

        const uint32_t materialSlotCount = gpuMesh->GetMaterialSlotCount();
        if (materialSlotCount == 0)
        {
            spdlog::error("Scene instantiate skipped mesh: GPU mesh contains no material slots.");
            entity.RemoveComponent<XJSurfaceMaterialComponent>();
            return false;
        }

        XJMaterialAssetRefComponent* materialRefs = nullptr;
        if (entity.HasComponent<XJMaterialAssetRefComponent>())
            materialRefs = &entity.GetComponent<XJMaterialAssetRefComponent>();
        else
            materialRefs = &entity.AddComponent<XJMaterialAssetRefComponent>();

        // 空引用表示该槽使用默认材质，同时保证 Inspector 显示全部槽位。
        materialRefs->Materials.resize(materialSlotCount);

        for(uint32_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
        {
            const XJSubmesh* submesh = gpuMesh->GetSubmesh(submeshIndex);

            if(!submesh)
                continue;

            // MaterialSlot 来自 glTF primitive metadata。
            auto material = CreateMaterialForSlot(data.MeshRenderer.Materials, submesh->MaterialSlot, ctx); 

            if (!material)
            {
                spdlog::error("Scene instantiate failed: material slot {} could not be created.", submesh->MaterialSlot);
                return false;
            }
            
            comp.AddMesh(gpuMesh, material, submeshIndex);
        }
        
        return comp.XJGetMeshCount() == submeshCount;
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

    bool XJSceneInstantiator::ApplyHierarchy(const XJSceneAsset& asset, XJSceneInstantiateContext& ctx)
    {
        if (HasHierarchyCycle(asset))
        {
            // 坏资产中的 A->B->A 会形成环。即使 XJNode 有运行时防护，
            // 实例化阶段也应直接拒绝恢复这批层级数据，避免留下半正确树结构。
            spdlog::error("Scene instantiate skipped hierarchy restore: cycle detected in scene asset.");
            return false;
        }

        for (const auto& entityData : asset.Entities)
        {
        
            if (entityData.Parent == 0)
                continue;

            if (entityData.Parent == entityData.UUID)
            {
                spdlog::warn("Scene instantiate skipped self-parent entity uuid={}", static_cast<uint64_t>(entityData.UUID));
                continue;
            }

            auto parentIt = ctx.EntityMap.find(entityData.Parent);
            auto childIt = ctx.EntityMap.find(entityData.UUID);
            if (parentIt == ctx.EntityMap.end() ||
                childIt == ctx.EntityMap.end() ||
                !parentIt->second ||
                !childIt->second)
            {
                return false;
            }

            parentIt->second->XJAddChild(childIt->second);
            
            if (childIt->second->XJGetParent() != parentIt->second)
            {
                return false;
            }
        }
        return true;
    }

    XJEntity* XJSceneInstantiator::FindInstantiatedEntity(const XJSceneInstantiateContext& ctx, XJUUID id) 
    {
        auto it = ctx.EntityMap.find(id);
        return (it != ctx.EntityMap.end()) ? it->second : nullptr;
    }

    void XJSceneInstantiator::ApplyLight(const XJSceneEntityData& data, XJEntity& entity)
    {
        auto& light = entity.AddComponent<XJLightComponent>();
        if (data.Light.UUID != 0)
            light.XJSetUUID(data.Light.UUID);

        light.XJSetEnable(data.Light.Enabled);
        light.XJSetLightType(static_cast<XJLightType>(data.Light.Type));
        light.XJSetColor(data.Light.Color);
        light.XJSetIntensity(data.Light.Intensity);
        light.XJSetRange(data.Light.Range);
        light.XJSetOuterAngleDegrees(data.Light.OuterAngleDegrees);
        light.XJSetInnerAngleDegrees(data.Light.InnerAngleDegrees);
    }
}
