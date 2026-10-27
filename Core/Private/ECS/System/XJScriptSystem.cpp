#include "ECS/System/XJScriptSystem.h"

#include "Asset/XJAssetRegistry.h"
#include "Asset/XJScriptAsset.h"
#include "ECS/Component/XJScriptComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"
#include "Script/Runtime/XJScriptHostNativeInvoker.h"
#include "Script/Host/XJEcsScriptEntityService.h"
#include "Script/Host/XJEcsScriptTransformService.h"
#include "Script/Host/XJScriptInputSnapshotService.h"
#include "Script/Host/XJSpdlogScriptLogService.h"

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
            XJAssetHandle ScriptHandle = 0;
            uint64_t LifecycleRevision = 0;

            std::shared_ptr<XJScriptAsset> Asset;
            XJScriptInstance Instance;

            bool Created = false;
            bool Active = false;
        };

        struct BusyGuard
        {
            explicit BusyGuard(bool& busy)
                : Busy(busy)
            {
                Busy = true;
            }

            ~BusyGuard()
            {
                Busy = false;
            }

            bool& Busy;
        };

        struct PendingScriptCreation
        {
            XJUUID EntityId{0};
            XJScriptSlot Source;
        };

        bool ApplyFieldOverrides(
            XJScriptInstance& instance,
            const XJScriptBytecodeModule& module,
            const XJScriptSlot& slot)
        {
            for (const auto& [fieldId, value] : slot.FieldOverrides)
            {
                const auto field = std::find_if(
                    module.Fields.begin(),
                    module.Fields.end(),
                    [fieldId](const auto& item)
                    {
                        return item.Access == XJScriptAccess::Public &&
                               item.StableId == fieldId &&
                               fieldId != 0;
                    });

                if (field == module.Fields.end())
                    return false;

                const auto index = static_cast<uint32_t>(
                    std::distance(module.Fields.begin(), field));

                if (!instance.SetField(index, value))
                    return false;
            }

            return true;
        }
    }

    class XJScriptSystem::Impl
    {
        public:

            XJScriptStatisticsRecorder Statistics;

            XJScene* Scene = nullptr;
            XJAssetRegistry* Registry = nullptr;
            std::weak_ptr<void> SceneLifetime;

            std::unordered_map<XJAssetHandle, XJScriptAssetCacheEntry> ScriptCache;
            XJScriptExecutionLimits Limits;

            std::shared_ptr<XJEcsScriptEntityService> EntityService;
            std::shared_ptr<XJScriptInputSnapshotService> InputService = std::make_shared<XJScriptInputSnapshotService>();
            std::unique_ptr<XJScriptHostNativeInvoker> DefaultInvoker;
            XJScriptNativeInvoker* Invoker = nullptr;
            uint64_t HostFrameId = 0;

            std::vector<RuntimeScriptSlot> RuntimeSlots;
            std::vector<XJEntity*> EntityScratch;
            std::vector<PendingScriptCreation> PendingCreations;

            // connection 本身不自动 disconnect。
            // Scene 已销毁时，不能对其 signal 执行 release。
            entt::connection DestroyConnection;

            bool Started = false;
            bool Busy = false;

            bool HasScene() const
            {
                return Scene && !SceneLifetime.expired();
            }
            void InitializeHost()//初始化主机
            {
                EntityService = std::make_shared<XJEcsScriptEntityService>(*Scene);

                auto transforms =
                    std::make_shared<XJEcsScriptTransformService>(
                        EntityService);

                auto log =
                    std::make_shared<XJSpdlogScriptLogService>();

                XJScriptHostServices services;

                services.Entities = EntityService;
                services.Transforms = std::move(transforms);
                services.Input = InputService;
                services.Log = std::move(log);

                DefaultInvoker =
                    std::make_unique<XJScriptHostNativeInvoker>(
                        std::move(services));

                // 自定义 Invoker 不覆盖。
                if (!Invoker)
                    Invoker = DefaultInvoker.get();

                InputService->Reset();
            }

            XJScriptNativeContext MakeNativeContext(//上下文构造函数
                XJEntity& owner,
                XJUUID slotId,
                XJScriptPhase phase)
            {
                XJScriptNativeContext context;

                context.Owner = &owner;

                context.Call.Owner =
                    EntityService->Capture(owner);

                context.Call.SlotId =
                    static_cast<uint64_t>(slotId);

                context.Call.FrameId = HostFrameId;
                context.Call.Phase = phase;

                return context;
            }


            const XJScriptSlot* FindSource(const RuntimeScriptSlot& runtime) const
            {
                if (!HasScene())
                    return nullptr;

                XJEntity* entity = Scene->FindEntityByUUID(runtime.EntityId);
                if (!entity || !entity->HasComponent<XJScriptComponent>())
                    return nullptr;

                return entity->GetComponent<XJScriptComponent>()
                    .FindSlot(runtime.SlotId);
            }

            bool MatchesSource(const RuntimeScriptSlot& runtime) const
            {
                const XJScriptSlot* source = FindSource(runtime);
                return source &&
                       source->Enabled &&
                       source->Script.Type == XJAssetType::Script &&
                       source->Script.Handle == runtime.ScriptHandle &&
                       source->LifecycleRevision == runtime.LifecycleRevision;
            }

            uint32_t CountActiveInstances() const
            {
                uint32_t count = 0;
                for (const auto& runtime : RuntimeSlots)
                {
                    if(runtime.Created && runtime.Active && MatchesSource(runtime))
                        ++count;
                }

                return count;
            }

            void DestroySlot(RuntimeScriptSlot& runtime) noexcept
            {
                runtime.Active = false;

                if (!runtime.Created)
                    return;

                // 先标记，再执行用户回调，确保清理最多执行一次。
                runtime.Created = false;

                if (!HasScene() || !runtime.Asset || !runtime.Asset->Module ||
                    !runtime.Asset->Module->OnDestroy)
                {
                    return;
                }

                XJEntity* owner = Scene->FindEntityByUUID(runtime.EntityId);
                if (!owner)
                    return;

                try
                {
                    // 终止实例不再恢复 Update，只允许最后一次清理。
                    XJScriptRuntime::ClearFault(runtime.Instance);

                    XJScene::ScriptExecutionScope executionScope(*Scene);
                    auto context = MakeNativeContext(*owner, runtime.SlotId, XJScriptPhase::Destroy);

                    const auto result = XJScriptRuntime::Execute(
                        runtime.Instance,
                        *runtime.Asset->Module->OnDestroy,
                        {},
                        *Invoker,
                        context,
                        Limits);

                    if (!result.IsValid())
                    {
                        spdlog::error(
                            "Script OnDestroy failed: entity={}, slot={}, {}",
                            static_cast<uint64_t>(runtime.EntityId),
                            static_cast<uint64_t>(runtime.SlotId),
                            result.Error->Message);
                    }
                }
                catch (const std::exception& exception)
                {
                    spdlog::error("Script cleanup failed: {}", exception.what());
                }
                catch (...)
                {
                    spdlog::error("Script cleanup failed with unknown exception.");
                }
            }
            void BeforeEntityDestroy(XJEntity& entity) noexcept
            {
                XJScriptStatisticsRecorder::Scope statisticsScope(Statistics);
                BusyGuard busyGuard(Busy);
                const XJUUID entityId = entity.XJGetUUID();

                for (size_t index = RuntimeSlots.size(); index > 0; --index)
                {
                    auto& runtime = RuntimeSlots[index - 1];
                    if (runtime.EntityId != entityId)
                        continue;

                    DestroySlot(runtime);
                    RuntimeSlots.erase(RuntimeSlots.begin() + (index - 1));
                }
            }

            void CreateSlot(
                XJEntity& entity,
                const XJScriptSlot& source,
                bool strict)
            {
                // 失败记录也保留，避免相同配置每帧重试/编译。
                RuntimeSlots.emplace_back();
                auto& runtime = RuntimeSlots.back();

                runtime.EntityId = entity.XJGetUUID();
                runtime.SlotId = source.SlotId;
                runtime.ScriptHandle = source.Script.Handle;
                runtime.LifecycleRevision = source.LifecycleRevision;

                try
                {
                    if (!source.Script.IsValid() ||
                        source.Script.Type != XJAssetType::Script)
                    {
                        throw std::runtime_error("Invalid Script asset reference");
                    }

                    XJScriptAssetLoadContext context{Registry, &ScriptCache};
                    runtime.Asset = XJScriptAssetLoader::LoadScript(
                        source.Script.Handle, context);

                    if (runtime.Asset && !runtime.Asset->IsCompiled() &&
                        !runtime.Asset->Diagnostics.empty())
                        throw std::runtime_error(runtime.Asset->Diagnostics.front().Message);
                    if (!runtime.Asset || !runtime.Asset->IsCompiled())
                        throw std::runtime_error("Script asset failed to compile");

                    const auto error = XJScriptRuntime::InitializeInstance(
                        runtime.Instance, runtime.Asset->Module);

                    if (error)
                        throw std::runtime_error(error->Message);

                    if (!ApplyFieldOverrides(
                            runtime.Instance, *runtime.Asset->Module, source))
                    {
                        throw std::runtime_error("Script field overrides are invalid");
                    }

                    // OnCreate 部分执行后失败，也需要尝试清理。
                    runtime.Created = true;

                    if (runtime.Asset->Module->OnCreate)
                    {
                        XJScene::ScriptExecutionScope executionScope(*Scene);
                        auto nativeContext = MakeNativeContext(
                            entity,
                            runtime.SlotId,
                            XJScriptPhase::Create);

                        const auto result = XJScriptRuntime::Execute(
                            runtime.Instance,
                            *runtime.Asset->Module->OnCreate,
                            {},
                            *Invoker,
                            nativeContext,
                            Limits);

                        if (!result.IsValid())
                            throw std::runtime_error(result.Error->Message);
                    }

                    runtime.Active = true;
                }
                catch (const std::exception& exception)
                {
                    DestroySlot(runtime);

                    spdlog::error(
                        "Runtime Script creation failed: entity={}, slot={}, {}",
                        static_cast<uint64_t>(runtime.EntityId),
                        static_cast<uint64_t>(runtime.SlotId),
                        exception.what());
                    if (strict)
                        throw;
                }
            }

            void Synchronize(bool strict)
            {
                XJScriptStatisticsRecorder::Scope statisticsScope(Statistics);
                if (!Started || !HasScene())
                    return;

                if (Busy)
                {
                    spdlog::error("Script synchronization is reentrant.");
                    throw std::logic_error("Script synchronization is reentrant");
                }

                BusyGuard busyGuard(Busy);

                // 移除、禁用、替换先清理旧实例。
                for (size_t index = RuntimeSlots.size(); index > 0; --index)
                {
                    if (MatchesSource(RuntimeSlots[index - 1]))
                        continue;

                    DestroySlot(RuntimeSlots[index - 1]);
                    RuntimeSlots.erase(RuntimeSlots.begin() + (index - 1));
                }

                EntityScratch.clear();
                PendingCreations.clear();

                for (const auto& [ecsEntity, entity] : Scene->GetEntities())
                {
                    (void)ecsEntity;
                    if (entity && entity->HasComponent<XJScriptComponent>())
                        EntityScratch.push_back(entity.get());
                }

                std::sort(
                    EntityScratch.begin(),
                    EntityScratch.end(),
                    [](const XJEntity* left, const XJEntity* right)
                    {
                        return static_cast<uint64_t>(left->XJGetUUID()) <
                               static_cast<uint64_t>(right->XJGetUUID());
                    });

                for (XJEntity* entity : EntityScratch)
                {
                    const auto& component =
                        entity->GetComponent<XJScriptComponent>();

                    for (const auto& source : component.GetSlots())
                    {
                        if (!source.Enabled)
                            continue;

                        const auto existing = std::find_if(
                            RuntimeSlots.begin(),
                            RuntimeSlots.end(),
                            [&](const RuntimeScriptSlot& runtime)
                            {
                                return runtime.EntityId == entity->XJGetUUID() &&
                                       runtime.SlotId == source.SlotId;
                            });

                        if (existing == RuntimeSlots.end())
                            PendingCreations.push_back({entity->XJGetUUID(), source});
                    }
                }

                // 用户 Native 回调可能修改 ScriptComponent，不能跨回调持有 slot iterator。
                for (const auto& pending : PendingCreations)
                {
                    XJEntity* entity = Scene->FindEntityByUUID(pending.EntityId);
                    if (!entity || !entity->HasComponent<XJScriptComponent>())
                        continue;
                    const auto* current = entity->GetComponent<XJScriptComponent>()
                        .FindSlot(pending.Source.SlotId);
                    if (!current || !current->Enabled ||
                        current->LifecycleRevision != pending.Source.LifecycleRevision ||
                        current->Script.Handle != pending.Source.Script.Handle ||
                        current->Script.Type != pending.Source.Script.Type)
                        continue;
                    CreateSlot(*entity, pending.Source, strict);
                }
                PendingCreations.clear();
            }

            void Tick(float deltaTime, bool fixed)
            {
                XJScriptStatisticsRecorder::Scope statisticsScope(Statistics);
                Synchronize(false);

                if (!Started || !HasScene())
                    return;

                BusyGuard busyGuard(Busy);
                const XJScriptValue argument = static_cast<double>(deltaTime);

                for (auto& runtime : RuntimeSlots)
                {
                    if (!runtime.Created || !runtime.Active || !MatchesSource(runtime))
                        continue;

                    const auto& module = *runtime.Asset->Module;
                    const auto function = fixed
                        ? module.OnFixedUpdate
                        : module.OnUpdate;

                    if (!function)
                        continue;

                    XJEntity* owner = Scene->FindEntityByUUID(runtime.EntityId);
                    if (!owner)
                        continue;

                    XJScene::ScriptExecutionScope executionScope(*Scene);
                    auto context = MakeNativeContext(*owner,runtime.SlotId,fixed ? XJScriptPhase::FixedUpdate : XJScriptPhase::Update);

                    const auto result = XJScriptRuntime::Execute(
                        runtime.Instance,
                        *function,
                        std::span<const XJScriptValue>(&argument, 1),
                        *Invoker,
                        context,
                        Limits);

                    if (!result.IsValid())
                    {
                        runtime.Active = false;
                        spdlog::error(
                            "Script {} failed: entity={}, slot={}, {}",
                            fixed ? "OnFixedUpdate" : "OnUpdate",
                            static_cast<uint64_t>(runtime.EntityId),
                            static_cast<uint64_t>(runtime.SlotId),
                            result.Error->Message);
                    }
                }
            }
    };

    XJScriptSystem::XJScriptSystem(
        XJScene& scene,
        XJAssetRegistry& registry,
        std::unordered_map<XJAssetHandle, XJScriptAssetCacheEntry> preparedCache,
        XJScriptExecutionLimits limits,
        XJScriptNativeInvoker* nativeInvoker)
        : mImpl(std::make_unique<Impl>())
    {
        mImpl->Scene = &scene;
        mImpl->SceneLifetime = scene.GetLifetimeToken();
        mImpl->Registry = &registry;
        mImpl->ScriptCache = std::move(preparedCache);
        mImpl->Limits = limits;

        if (nativeInvoker)
            mImpl->Invoker = nativeInvoker;
    }

    XJScriptSystem::~XJScriptSystem()
    {
        OnDestroy();
    }

    void XJScriptSystem::OnCreate()
    {
        XJScriptStatisticsRecorder::Scope statisticsScope(mImpl->Statistics);

        if (mImpl->Busy)
        {
            spdlog::error("ScriptSystem lifecycle is reentrant.");
            throw std::logic_error("ScriptSystem lifecycle is reentrant");
        }
        if (mImpl->Started)
        {
            spdlog::error("ScriptSystem is already started.");
            throw std::logic_error("ScriptSystem is already started");
        }

        if (!mImpl->HasScene())
        {
            spdlog::error("ScriptSystem Scene is unavailable.");
            throw std::runtime_error("ScriptSystem Scene is unavailable");
        }

        mImpl->InitializeHost();
        mImpl->Started = true;
        mImpl->DestroyConnection =
            mImpl->Scene->BeforeEntityDestroy()
                .connect<&Impl::BeforeEntityDestroy>(*mImpl);

        try
        {
            mImpl->Synchronize(true);
        }
        catch (...)
        {
            OnDestroy();
            throw;
        }
    }

    void XJScriptSystem::SynchronizeSlots()
    {
        mImpl->Synchronize(false);
    }

    void XJScriptSystem::OnUpdate(float deltaTime)
    {
        mImpl->Tick(deltaTime, false);
    }

    void XJScriptSystem::PublishInput(
        const XJInputState& input,
        bool enabled)
    {
        if (mImpl->Busy)
        {
            spdlog::error("Cannot publish script input during execution.");
            throw std::logic_error("Script input publication is reentrant");
        }

        mImpl->InputService->Publish(input, enabled && mImpl->Started);
    }

    void XJScriptSystem::OnFixedUpdate(float fixedDeltaTime)
    {
        mImpl->Tick(fixedDeltaTime, true);
    }

    void XJScriptSystem::OnDestroy()
    {
        XJScriptStatisticsRecorder::Scope statisticsScope(mImpl->Statistics);

        if (mImpl->Busy)
        {
            spdlog::error("ScriptSystem destruction is reentrant.");
            throw std::logic_error("ScriptSystem destruction is reentrant");
        }
        if (!mImpl->Started)
            return;

        BusyGuard busyGuard(mImpl->Busy);
        mImpl->Started = false;

        if (mImpl->HasScene())
            mImpl->DestroyConnection.release();

        for (auto it = mImpl->RuntimeSlots.rbegin();
             it != mImpl->RuntimeSlots.rend(); ++it)
        {
            mImpl->DestroySlot(*it);
        }

        mImpl->RuntimeSlots.clear();
        mImpl->EntityScratch.clear();
        mImpl->PendingCreations.clear();

        // 必须在所有 OnDestroy 执行完之后释放服务。
        if (mImpl->Invoker == mImpl->DefaultInvoker.get())
            mImpl->Invoker = nullptr;

        mImpl->DefaultInvoker.reset();
        mImpl->EntityService.reset();
        mImpl->InputService->Reset();
    }

    void XJScriptSystem::BeginStatisticsFrame(uint64_t frameId)
    {
        if (mImpl->Busy)
        {
            spdlog::error("Cannot reset ScriptSystem statistics during execution.");
            throw std::logic_error("ScriptSystem statistics frame is reentrant");
        }
        mImpl->HostFrameId = frameId;

        mImpl->Statistics.BeginFrame(frameId);
    }

    XJScriptStatistics XJScriptSystem::GetStatistics() const
    {
        return mImpl->Statistics.GetSnapshot(
            mImpl->CountActiveInstances());
    }
}
