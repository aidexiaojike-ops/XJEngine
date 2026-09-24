#include "ECS/System/XJScriptSystem.h"

#include "Asset/XJAssetRegistry.h"
#include "Asset/XJScriptAsset.h"
#include "ECS/Component/XJScriptComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"
#include "Script/Runtime/XJEcsScriptNativeInvoker.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace XJ
{
    namespace
    {
        struct RuntimeScriptSlot
        {
            XJUUID EntityId{0};
            XJUUID SlotId{0};
            std::shared_ptr<XJScriptAsset> Asset;
            XJScriptInstance Instance;
            bool OnCreateCompleted = false;
            bool Active = true;
        };

        bool ApplyFieldOverrides(
            XJScriptInstance& instance,
            const XJScriptBytecodeModule& module,
            const XJScriptSlot& slot)
        {
            std::unordered_map<uint64_t, uint32_t> fields;
            for (uint32_t index = 0; index < module.Fields.size(); ++index)
            {
                const auto& field = module.Fields[index];
                if (field.Access == XJScriptAccess::Public && field.StableId != 0)
                    fields[field.StableId] = index;
            }

            for (const auto& [fieldId, value] : slot.FieldOverrides)
            {
                const auto field = fields.find(fieldId);
                if (field == fields.end() || !instance.SetField(field->second, value))
                    return false;
            }
            return true;
        }
    }

    class XJScriptSystem::Impl
    {
    public:
        XJScene* Scene = nullptr;
        XJAssetRegistry* Registry = nullptr;
        std::unordered_map<XJAssetHandle, XJScriptAssetCacheEntry> ScriptCache;
        XJScriptExecutionLimits Limits;
        XJEcsScriptNativeInvoker NativeInvoker;
        std::vector<RuntimeScriptSlot> RuntimeSlots;
    };

    XJScriptSystem::XJScriptSystem(
        XJScene& scene,
        XJAssetRegistry& registry,
        std::unordered_map<XJAssetHandle, XJScriptAssetCacheEntry> preparedCache,
        XJScriptExecutionLimits limits)
        : mImpl(std::make_unique<Impl>())
    {
        mImpl->Scene = &scene;
        mImpl->Registry = &registry;
        mImpl->ScriptCache = std::move(preparedCache);
        mImpl->Limits = limits;
    }

    XJScriptSystem::~XJScriptSystem() = default;

    void XJScriptSystem::OnCreate()
    {
        std::vector<XJEntity*> entities;
        for (const auto& [enttEntity, entity] : mImpl->Scene->GetEntities())
        {
            (void)enttEntity;
            if (entity && entity->HasComponent<XJScriptComponent>())
                entities.push_back(entity.get());
        }
        std::sort(entities.begin(), entities.end(),
            [](const XJEntity* left, const XJEntity* right)
            {
                return static_cast<uint64_t>(left->XJGetUUID()) <
                       static_cast<uint64_t>(right->XJGetUUID());
            });

        XJScriptAssetLoadContext loadContext{mImpl->Registry, &mImpl->ScriptCache};
        for (XJEntity* entity : entities)
        {
            const auto& component = entity->GetComponent<XJScriptComponent>();
            for (const auto& slot : component.GetSlots())
            {
                if (!slot.Enabled)
                    continue;
                auto asset = XJScriptAssetLoader::LoadScript(slot.Script.Handle, loadContext);
                if (!asset || !asset->IsCompiled())
                    throw std::runtime_error("Script asset failed to compile");

                RuntimeScriptSlot runtime;
                runtime.EntityId = entity->XJGetUUID();
                runtime.SlotId = slot.SlotId;
                runtime.Asset = std::move(asset);
                const auto error = XJScriptRuntime::InitializeInstance(
                    runtime.Instance, runtime.Asset->Module);
                if (error)
                    throw std::runtime_error(error->Message);
                if (!ApplyFieldOverrides(runtime.Instance, *runtime.Asset->Module, slot))
                    throw std::runtime_error("Script field overrides are invalid");
                mImpl->RuntimeSlots.push_back(std::move(runtime));
            }
        }

        for (auto& runtime : mImpl->RuntimeSlots)
        {
            runtime.OnCreateCompleted = true;
            if (!runtime.Asset->Module->OnCreate)
                continue;
            XJScriptNativeContext context{
                mImpl->Scene->FindEntityByUUID(runtime.EntityId)
            };
            const auto result = XJScriptRuntime::Execute(
                runtime.Instance, *runtime.Asset->Module->OnCreate, {},
                mImpl->NativeInvoker, context, mImpl->Limits);
            if (!result.IsValid())
            {
                runtime.OnCreateCompleted = false;
                throw std::runtime_error(result.Error->Message);
            }
        }
    }

    void XJScriptSystem::OnUpdate(float deltaTime)
    {
        const XJScriptValue argument = static_cast<double>(deltaTime);
        for (auto& runtime : mImpl->RuntimeSlots)
        {
            if (!runtime.Active || !runtime.OnCreateCompleted ||
                !runtime.Asset->Module->OnUpdate)
                continue;
            XJEntity* owner = mImpl->Scene->FindEntityByUUID(runtime.EntityId);
            if (!owner)
            {
                runtime.Active = false;
                continue;
            }
            XJScriptNativeContext context{owner};
            const auto result = XJScriptRuntime::Execute(
                runtime.Instance, *runtime.Asset->Module->OnUpdate,
                std::span<const XJScriptValue>(&argument, 1),
                mImpl->NativeInvoker, context, mImpl->Limits);
            if (!result.IsValid())
            {
                runtime.Active = false;
                spdlog::error("Script OnUpdate failed: {}", result.Error->Message);
            }
        }
    }

    void XJScriptSystem::OnFixedUpdate(float fixedDeltaTime)
    {
        const XJScriptValue argument = static_cast<double>(fixedDeltaTime);
        for (auto& runtime : mImpl->RuntimeSlots)
        {
            if (!runtime.Active || !runtime.OnCreateCompleted ||
                !runtime.Asset->Module->OnFixedUpdate)
                continue;
            XJEntity* owner = mImpl->Scene->FindEntityByUUID(runtime.EntityId);
            if (!owner)
            {
                runtime.Active = false;
                continue;
            }
            XJScriptNativeContext context{owner};
            const auto result = XJScriptRuntime::Execute(
                runtime.Instance, *runtime.Asset->Module->OnFixedUpdate,
                std::span<const XJScriptValue>(&argument, 1),
                mImpl->NativeInvoker, context, mImpl->Limits);
            if (!result.IsValid())
            {
                runtime.Active = false;
                spdlog::error("Script OnFixedUpdate failed: {}", result.Error->Message);
            }
        }
    }

    void XJScriptSystem::OnDestroy()
    {
        for (auto it = mImpl->RuntimeSlots.rbegin(); it != mImpl->RuntimeSlots.rend(); ++it)
        {
            auto& runtime = *it;
            if (!runtime.OnCreateCompleted || !runtime.Active ||
                runtime.Instance.IsFaulted() || !runtime.Asset->Module->OnDestroy)
                continue;
            XJScriptNativeContext context{
                mImpl->Scene->FindEntityByUUID(runtime.EntityId)
            };
            const auto result = XJScriptRuntime::Execute(
                runtime.Instance, *runtime.Asset->Module->OnDestroy, {},
                mImpl->NativeInvoker, context, mImpl->Limits);
            if (!result.IsValid())
                spdlog::error("Script OnDestroy failed: {}", result.Error->Message);
        }
        mImpl->RuntimeSlots.clear();
    }
}
