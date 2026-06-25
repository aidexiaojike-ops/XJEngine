#include "Services/XJEditorSceneService.h"
#include <optional>

#include "ECS/XJEntity.h"
#include "ECS/XJNode.h"
#include "ECS/XJScene.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"

#include "Asset/Loader/XJMeshAssetLoader.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/Instantiation/XJSceneInstantiator.h"
//材质
#include "Render/Resource/XJMaterialFactory.h"
#include "Asset/Importer/XJMaterialImporter.h"
//材质参数
#include "Asset/Serialization/XJMaterialAssetSerializer.h"
#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Render/Shader/XJShaderParameter.h"

#include <algorithm>
#include <unordered_set>

namespace XJ
{
    namespace
    {
        bool IsValidMeshAsset(XJAssetRegistry& assetRegistry, XJAssetHandle meshAsset)//是否有模型资产
        {
            if (meshAsset == 0)
                return false;

            auto meta = assetRegistry.GetMeta(meshAsset);
            return meta.has_value() && meta->Type == XJAssetType::Mesh;
        }

        bool IsValidMaterialAsset(XJAssetRegistry& assetRegistry, XJAssetHandle materialAsset)//是否有材质资产
        {
            if(materialAsset == 0)
                return false;
            
            auto meta = assetRegistry.GetMeta(materialAsset);
            return meta.has_value() && meta->Type == XJAssetType::Material;
        }

        void EnsureTransformComponent(XJEntity& entity)
        {
            if (entity.HasComponent<XJTransformComponent>())
                return;

            auto& transform = entity.AddComponent<XJTransformComponent>();
            transform.position = glm::vec3(0.0f);
            transform.rotation = glm::vec3(0.0f);
            transform.scale = glm::vec3(1.0f);
            transform.UpdateModelMatrix();
        }

        std::shared_ptr<XJUnlitMaterial> CreateMaterialForSlot(XJEntity& entity, uint32_t slotIndex, XJAssetRegistry& assetRegistry, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)//创建材质插槽
        {
            if (entity.HasComponent<XJMaterialAssetRefComponent>())
            {
                const auto& materialRef = entity.GetComponent<XJMaterialAssetRefComponent>();

                if (slotIndex < materialRef.Materials.size())
                {
                    const auto& materialAssetRef = materialRef.Materials[slotIndex];

                    if (materialAssetRef.IsValid())
                    {
                        auto meta = assetRegistry.GetMeta(materialAssetRef.Handle);
                        if (meta && meta->Type == XJAssetType::Material)
                        {
                            auto materialAsset = XJMaterialImporter::ImportMaterial(meta->SourcePath.string());
                            if (materialAsset)
                            {
                                materialAsset->mHandle = meta->Handle;
                                materialAsset->mName = meta->Name;
                                materialAsset->mPath = meta->SourcePath;
                            
                                return XJMaterialFactory::GetInstance()->CreateFromAsset(*materialAsset, defaultTexture, defaultSampler);
                            }
                        }
                    }
                }
            }

            return XJMaterialFactory::GetInstance()->CreateDefaultMaterial(defaultTexture, defaultSampler);
        }

        bool RebuildUnlitMeshRenderData(XJEntity& entity, XJAssetHandle meshAsset, XJAssetRegistry& assetRegistry, XJSceneInstantiateContext& instantiateContext, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)//设置默认材质
        {
            if (!defaultTexture || !defaultSampler)
                return false;

            XJMeshAssetLoadContext loadContext;
            loadContext.Registry = &assetRegistry;
            loadContext.MeshCache = &instantiateContext.MeshCache;

            std::shared_ptr<XJMesh> gpuMesh = XJMeshAssetLoader::LoadMesh(meshAsset, loadContext);
            if (!gpuMesh)
                return false;
                
            auto material = CreateMaterialForSlot(entity, 0, assetRegistry, defaultTexture, defaultSampler);
            if (!material)    
                return false;

            if (entity.HasComponent<XJUnlitMaterialComponent>())
                entity.RemoveComponent<XJUnlitMaterialComponent>();


            auto& renderComponent = entity.AddComponent<XJUnlitMaterialComponent>();
            renderComponent.AddMesh(gpuMesh.get(), material.get());

            return true;
        }

        std::optional<XJEditorEntityView> BuildEntityViewRecursive(XJEntity* entity, const XJEditorSceneService::ShouldExposeEntityCallback& shouldExposeEntity)
        {
            if (!entity)
                return std::nullopt;

            XJEditorEntityId entityId = static_cast<XJEditorEntityId>(entity->XJGetUUID());

            if (shouldExposeEntity && !shouldExposeEntity(entityId))
                return std::nullopt;

            XJEditorEntityView view;
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
                {
                    auto childView = BuildEntityViewRecursive(childEntity, shouldExposeEntity);
                    if (childView.has_value())
                        view.Children.push_back(std::move(childView.value()));
                }
            }

            return view;
        }

        XJEditorMaterialParameterType ToEditorMaterialParameterType(XJShaderParameterType type)//编辑参数类型
        {
            switch (type)
            {
                case XJShaderParameterType::Float: return XJEditorMaterialParameterType::Float;
                case XJShaderParameterType::Int: return XJEditorMaterialParameterType::Int;
                case XJShaderParameterType::Bool: return XJEditorMaterialParameterType::Bool;
                case XJShaderParameterType::Vec2: return XJEditorMaterialParameterType::Vec2;
                case XJShaderParameterType::Vec3: return XJEditorMaterialParameterType::Vec3;
                case XJShaderParameterType::Vec4: return XJEditorMaterialParameterType::Vec4;
                case XJShaderParameterType::Color3: return XJEditorMaterialParameterType::Color3;
                case XJShaderParameterType::Color4: return XJEditorMaterialParameterType::Color4;
                case XJShaderParameterType::Texture2D: return XJEditorMaterialParameterType::Texture2D;
                default: return XJEditorMaterialParameterType::None;
            }
        }

        XJEditorMaterialParameterValue ToEditorMaterialParameterValue(const XJMaterialParameterValue& value)//编辑参数值
        {
            return std::visit(
                [](const auto& v) -> XJEditorMaterialParameterValue
                {
                    return v;
                },
                value);
        }

        XJMaterialParameterValue ToRuntimeMaterialParameterValue(const XJEditorMaterialParameterValue& value)//运行时材质参数值
        {
            return std::visit(
                [](const auto& v) -> XJMaterialParameterValue
                {
                    return v;
                },
                value);
        }

        void PopulateMaterialSlotParameters(XJEditorMaterialSlotView& slot, XJAssetRegistry& assetRegistry)
        {
            if (!slot.HasMaterialAsset || slot.MaterialAsset == 0)
                return;
        
            auto meta = assetRegistry.GetMeta(slot.MaterialAsset);
            if (!meta || meta->Type != XJAssetType::Material)
                return;
        
            slot.MaterialPath = meta->SourcePath;
        
            auto materialAsset = XJMaterialImporter::ImportMaterial(meta->SourcePath.string());
            if (!materialAsset)
                return;
        
            slot.ShaderPath = materialAsset->ShaderPath;
        
            if (materialAsset->ShaderPath.empty())
                return;
        
            auto shaderAsset = XJShaderAssetSerializer::LoadFromFile(materialAsset->ShaderPath);
            if (!shaderAsset)
                return;
        
            for (const auto& def : shaderAsset->Schema.Parameters)
            {
                XJEditorMaterialParameterView parameter;
                parameter.Name = def.Name;
                parameter.DisplayName = def.DisplayName.empty() ? def.Name : def.DisplayName;
                parameter.Type = ToEditorMaterialParameterType(def.Type);
                parameter.Editable = def.Editable;
                parameter.HasRange = def.HasRange;
                parameter.Min = def.Min;
                parameter.Max = def.Max;
                parameter.Category = def.Category;
                parameter.IsOverride = materialAsset->HasParameterOverride(def.Name);

                if (const XJMaterialParameterValue* value = materialAsset->FindParameter(def.Name))
                    parameter.Value = ToEditorMaterialParameterValue(*value);
                else
                    parameter.Value = ToEditorMaterialParameterValue(def.DefaultValue);
            
                slot.Parameters.push_back(parameter);
            }
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

    XJEditorSceneViewModel XJEditorSceneService::BuildSceneViewModel(XJScene& scene, const ShouldExposeEntityCallback& shouldExposeEntity)
    {
        XJEditorSceneViewModel viewModel;

        XJNode* root = scene.XJGetRootNode();
        if (!root)
            return viewModel;

        for (XJNode* child : root->XJGetChildren())
        {
            if (XJEntity* entity = dynamic_cast<XJEntity*>(child))
            {
                auto entityView = BuildEntityViewRecursive(entity, shouldExposeEntity);
                if (entityView.has_value())
                    viewModel.RootEntities.push_back(std::move(entityView.value()));
            }
        }

        return viewModel;
    }

    XJEditorEntityDetailsView XJEditorSceneService::BuildEntityDetailsView(XJScene& scene, XJEditorEntityId entityId, XJAssetRegistry* assetRegistry, const ShouldExposeEntityCallback& shouldExposeEntity)
    {
        XJEditorEntityDetailsView details;

        if (shouldExposeEntity && !shouldExposeEntity(entityId))
            return details;


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

            XJEditorMaterialSlotView defaultSlot;
            defaultSlot.SlotIndex = 0;
            defaultSlot.HasMaterialAsset = false;
            defaultSlot.MaterialAsset = 0;
            defaultSlot.MaterialUri.clear();
            defaultSlot.DisplayName = "Default Unlit Material";
            details.Mesh.MaterialSlots.push_back(defaultSlot);

            if(entity->HasComponent<XJMaterialAssetRefComponent>())//获取mesh组件上的材质组件
            {
                const auto& materialRef = entity->GetComponent<XJMaterialAssetRefComponent>();

                for(size_t i = 0; i < materialRef.Materials.size(); ++i)//循环材质组件所有的容器  分别设置参数
                {
                    const auto& material = materialRef.Materials[i];

                    if (i >= details.Mesh.MaterialSlots.size())
                    {
                        XJEditorMaterialSlotView slot;
                        slot.SlotIndex = static_cast<uint32_t>(i);
                        slot.DisplayName = "Default Unlit Material";
                        details.Mesh.MaterialSlots.push_back(slot);
                    }
                
                    auto& slot = details.Mesh.MaterialSlots[i];
                    slot.HasMaterialAsset = material.IsValid();
                    slot.MaterialAsset = material.IsValid() ? material.Handle : 0;
                    slot.MaterialUri = material.IsValid() ? material.ToUri() : std::string{};
                    slot.DisplayName = material.IsValid() ? material.ToUri() : "Default Unlit Material";

                    if (assetRegistry && slot.HasMaterialAsset)
                        PopulateMaterialSlotParameters(slot, *assetRegistry);
                }
            }

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

            case XJEditorComponentType::Camera://添加摄像机也要添加transform
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

    bool XJEditorSceneService::DeleteComponent(XJScene& scene, XJEditorEntityId entityId, XJEditorComponentType componentType)
    {
        XJEntity* entity = FindEntityById(scene, entityId);

        if(!entity || !entity->IsValid())
            return false;

        switch(componentType)
        {
            case XJEditorComponentType::Transform:
            {
                if(!entity->HasComponent<XJTransformComponent>())
                    return false;

                entity->RemoveComponent<XJTransformComponent>();
                return true;
            }
            case XJEditorComponentType::Camera:
            {
                if(!entity->HasComponent<XJCameraComponent>())
                    return false;

                entity->RemoveComponent<XJCameraComponent>();
                    return true;
            }
            case XJEditorComponentType::MeshRenderer:
            {
                bool removed = false;//移除模型也要移除材质

                if(entity->HasComponent<XJMeshAssetRefComponent>())
                {
                    entity->RemoveComponent<XJMeshAssetRefComponent>();
                    removed = true;
                }

                if(entity->HasComponent<XJMaterialAssetRefComponent>())
                {
                    entity->RemoveComponent<XJMaterialAssetRefComponent>();
                    removed = true;
                }

                if(entity->HasComponent<XJUnlitMaterialComponent>())
                {
                   entity->RemoveComponent<XJUnlitMaterialComponent>();
                   removed = true;
                }

                return removed;
            }
            case XJEditorComponentType::SceneAssetRef:
            {
                if(!entity->HasComponent<XJSceneAssetRefComponent>())
                    return false;

                entity->RemoveComponent<XJSceneAssetRefComponent>();
                    return true;
            }

            default:
                return false;
        }
    }

    bool XJEditorSceneService::AddMeshRendererComponent(XJScene& scene, XJEditorEntityId entityId, XJAssetHandle defaultMeshAsset, XJAssetRegistry& assetRegistry, XJSceneInstantiateContext& instantiateContext, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)
    {
        XJEntity* entity = FindEntityById(scene, entityId);
        if(!entity || !entity->IsValid())
            return false;

        if(entity->HasComponent<XJMeshAssetRefComponent>())
            return false;
        
        if(!IsValidMeshAsset(assetRegistry, defaultMeshAsset))
            return false;

        EnsureTransformComponent(*entity);
        if (!RebuildUnlitMeshRenderData(*entity, defaultMeshAsset, assetRegistry, instantiateContext, defaultTexture, defaultSampler))
        {
            return false;
        }

        auto& meshRef = entity->AddComponent<XJMeshAssetRefComponent>();
        meshRef.Mesh = XJAssetRef{ defaultMeshAsset, XJAssetType::Mesh };

        return true;
    }

    bool XJEditorSceneService::SetMeshRendererMesh(XJScene& scene, XJEditorEntityId entityId, XJAssetHandle meshAsset, XJAssetRegistry& assetRegistry, XJSceneInstantiateContext& instantiateContext, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)
    {
         XJEntity* entity = FindEntityById(scene, entityId);
        if (!entity || !entity->IsValid())
            return false;

        if (!entity->HasComponent<XJMeshAssetRefComponent>())
            return false;

        if (!IsValidMeshAsset(assetRegistry, meshAsset))
            return false;

        if (!RebuildUnlitMeshRenderData(*entity, meshAsset, assetRegistry, instantiateContext, defaultTexture, defaultSampler))
        {
            return false;
        }

        auto& meshRef = entity->GetComponent<XJMeshAssetRefComponent>();
        meshRef.Mesh = XJAssetRef{ meshAsset, XJAssetType::Mesh };

        return true;
    }

    bool XJEditorSceneService::SetMeshRendererMaterial(XJScene& scene, XJEditorEntityId entityId, uint32_t slotIndex, XJAssetHandle materialAsset, XJAssetRegistry& assetRegistry, XJSceneInstantiateContext& instantiateContext, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler) 
    {
        XJEntity* entity = FindEntityById(scene, entityId);
        if (!entity || !entity->IsValid())
            return false;

        if (!entity->HasComponent<XJMeshAssetRefComponent>())
            return false;

        if (!IsValidMaterialAsset(assetRegistry, materialAsset))
            return false;

        const auto& meshRef = entity->GetComponent<XJMeshAssetRefComponent>();
        if (!meshRef.Mesh.IsValid())
            return false;

        if (!entity->HasComponent<XJMaterialAssetRefComponent>())
            entity->AddComponent<XJMaterialAssetRefComponent>();

        auto& materialRef = entity->GetComponent<XJMaterialAssetRefComponent>();

        if (materialRef.Materials.size() <= slotIndex)
            materialRef.Materials.resize(static_cast<size_t>(slotIndex) + 1);

        XJAssetRef oldMaterial = materialRef.Materials[slotIndex];//材质插槽

        materialRef.Materials[slotIndex] = XJAssetRef{ materialAsset, XJAssetType::Material };

        if (!RebuildUnlitMeshRenderData(*entity, meshRef.Mesh.Handle, assetRegistry, instantiateContext, defaultTexture, defaultSampler))
        {
            materialRef.Materials[slotIndex] = oldMaterial;
            RebuildUnlitMeshRenderData(*entity, meshRef.Mesh.Handle, assetRegistry, instantiateContext, defaultTexture, defaultSampler);

            return false;
        }

        return true;
    }
    bool XJEditorSceneService::ResetMeshRendererMaterialToDefault(XJScene& scene, XJEditorEntityId entityId, uint32_t slotIndex, XJAssetRegistry& assetRegistry, XJSceneInstantiateContext& instantiateContext, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)
    {
        XJEntity* entity = FindEntityById(scene, entityId);
        if (!entity || !entity->IsValid())
            return false;

        if (!entity->HasComponent<XJMeshAssetRefComponent>())
            return false;

        const auto& meshRef = entity->GetComponent<XJMeshAssetRefComponent>();
        if (!meshRef.Mesh.IsValid())
            return false;

        if (!entity->HasComponent<XJMaterialAssetRefComponent>())
            return false;

        auto& materialRef = entity->GetComponent<XJMaterialAssetRefComponent>();

        if (slotIndex >= materialRef.Materials.size())
            return false;

        XJAssetRef oldMaterial = materialRef.Materials[slotIndex];

        materialRef.Materials[slotIndex] = {};

        if (!RebuildUnlitMeshRenderData(*entity, meshRef.Mesh.Handle, assetRegistry, instantiateContext, defaultTexture, defaultSampler))
        {
            materialRef.Materials[slotIndex] = oldMaterial;
            RebuildUnlitMeshRenderData(*entity, meshRef.Mesh.Handle, assetRegistry, instantiateContext, defaultTexture, defaultSampler);

            return false;
        }

        return true;
    }

    bool XJEditorSceneService::SetMaterialParameter( XJScene& scene, XJEditorEntityId entityId, uint32_t slotIndex, XJAssetHandle materialAsset, const std::string& parameterName, const XJEditorMaterialParameterValue& value,
                            XJAssetRegistry& assetRegistry, XJSceneInstantiateContext& instantiateContext, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)
    {
        if(materialAsset == 0 || parameterName.empty())
            return false;

        auto meta = assetRegistry.GetMeta(materialAsset);
        if(!meta || meta->Type != XJAssetType::Material)
            return false;

        auto material = XJMaterialImporter::ImportMaterial(meta->SourcePath.string());
        if(!material)
            return false;

        material->mHandle = meta->Handle;
        material->mName = meta->Name;
        material->mPath = meta->SourcePath;
        material->SetParameterOverride(parameterName, ToRuntimeMaterialParameterValue(value));

        if (!XJMaterialAssetSerializer::SaveToFile(*material, meta->SourcePath))
            return false;

        return SetMeshRendererMaterial(scene, entityId, slotIndex, materialAsset, assetRegistry, instantiateContext, defaultTexture, defaultSampler);
    }
    
    bool XJEditorSceneService::ResetMaterialParameter(XJScene& scene, XJEditorEntityId entityId, uint32_t slotIndex, XJAssetHandle materialAsset, const std::string& parameterName,
                            XJAssetRegistry& assetRegistry, XJSceneInstantiateContext& instantiateContext, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler)
    {   
        if(materialAsset == 0 || parameterName.empty())
        {
            return false;
        }

        auto meta = assetRegistry.GetMeta(materialAsset);
        if(!meta || meta->Type != XJAssetType::Material)
            return false;
        
        auto material = XJMaterialImporter::ImportMaterial(meta->SourcePath.string());
        if(!material)
            return false;

        material->mHandle = meta->Handle;
        material->mName = meta->Name;
        material->mPath = meta->SourcePath;

        material->ClearParameterOverride(parameterName);

        auto shaderAsset = XJShaderAssetSerializer::LoadFromFile(material->ShaderPath);
        if(shaderAsset)
        {
            if(const XJParameterDef* def = shaderAsset->Schema.FindParameter(parameterName))
                material->Parameters[parameterName] = def->DefaultValue;
        }

        if(!XJMaterialAssetSerializer::SaveToFile(*material, meta->SourcePath))
            return false;

        return SetMeshRendererMaterial(scene, entityId, slotIndex, materialAsset, assetRegistry, instantiateContext, defaultTexture, defaultSampler);

    }
}