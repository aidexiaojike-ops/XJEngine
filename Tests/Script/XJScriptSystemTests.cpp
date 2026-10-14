#include "Asset/XJAssetRegistry.h"

#include "ECS/System/XJScriptSystem.h"
#include "ECS/Component/XJScriptComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"

#include "Script/Runtime/XJEcsScriptNativeInvoker.h"

#include <vector>
#include <functional>
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include <string_view>

namespace
{
    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }
    class RecordingInvoker final : public XJ::XJScriptNativeInvoker
{
public:
    XJ::XJEcsScriptNativeInvoker Ecs;
    int DestroyCalls = 0;
    bool DestroyOwnersValid = true;
    std::function<void(XJ::XJScriptNativeContext&, double)> Callback;

    XJ::XJScriptNativeInvokeResult Invoke(
        XJ::XJScriptNativeFunctionId functionId,
        std::span<const XJ::XJScriptValue> arguments,
        XJ::XJScriptNativeContext& context) override
    {
        if (functionId == XJ::XJScriptNativeFunctionId::TransformRotateY &&
            arguments.size() == 1 &&
            std::holds_alternative<double>(arguments[0]))
        {
            const double angle = std::get<double>(arguments[0]);

            if (angle == 4.0 || angle == 40.0)
            {
                ++DestroyCalls;
                DestroyOwnersValid &=
                    context.Owner &&
                    context.Owner->IsValid() &&
                    context.Owner->HasComponent<XJ::XJTransformComponent>();
            }

            if (Callback)
                Callback(context, angle);
        }

        return Ecs.Invoke(functionId, arguments, context);
    }
};

void TestDynamicLifecycle(
    XJ::XJAssetRegistry& registry,
    XJ::XJAssetHandle first,
    XJ::XJAssetHandle replacement,
    XJ::XJAssetHandle faulted)
{
    XJ::XJScene scene;
    RecordingInvoker invoker;

    auto* entity = scene.CreateEntityWithTransform("Dynamic");
    auto& component = entity->AddComponent<XJ::XJScriptComponent>();

    Check(component.AddSlotWithId(
              XJ::XJUUID{2001}, {first, XJ::XJAssetType::Script}) != nullptr,
          "initial dynamic slot added");

    XJ::XJScriptSystem system(scene, registry, {}, {}, &invoker);
    system.OnCreate();

    Check(component.SetSlotEnabled(XJ::XJUUID{2001}, false),
          "slot disabled");
    system.SynchronizeSlots();
    Check(invoker.DestroyCalls == 1, "disable calls OnDestroy");

    component.SetSlotEnabled(XJ::XJUUID{2001}, true);
    system.SynchronizeSlots();

    component.SetSlotScript(
        XJ::XJUUID{2001}, {replacement, XJ::XJAssetType::Script});
    system.SynchronizeSlots();
    Check(invoker.DestroyCalls == 2, "replacement destroys old instance");

    component.RemoveSlot(XJ::XJUUID{2001});
    system.SynchronizeSlots();
    Check(invoker.DestroyCalls == 3, "slot removal destroys replacement");

    component.AddSlotWithId(
        XJ::XJUUID{2001}, {first, XJ::XJAssetType::Script});
    system.SynchronizeSlots();

    entity->RemoveComponent<XJ::XJScriptComponent>();
    system.SynchronizeSlots();
    Check(invoker.DestroyCalls == 4, "component removal destroys instance");

    auto* faultEntity = scene.CreateEntityWithTransform("Faulted");
    faultEntity->AddComponent<XJ::XJScriptComponent>()
        .AddSlot({faulted, XJ::XJAssetType::Script});

    system.SynchronizeSlots();
    system.OnUpdate(0.1f);

    scene.DestroyEntity(faultEntity);
    Check(invoker.DestroyCalls == 5, "faulted instance still receives cleanup");
    Check(invoker.DestroyOwnersValid, "cleanup runs before owner components vanish");

    auto* queued = scene.CreateEntityWithTransform("Queued");
    queued->AddComponent<XJ::XJScriptComponent>()
        .AddSlot({first, XJ::XJAssetType::Script});
    system.SynchronizeSlots();

    scene.QueueDestroyEntity(queued);
    Check(invoker.DestroyCalls == 5, "queued deletion waits for safe point");

    scene.FlushDestroyQueue();
    Check(invoker.DestroyCalls == 6, "queued deletion calls cleanup");

    auto* parent = scene.CreateEntityWithTransform("Parent");
    auto* child = scene.CreateEntityWithTransform("Child");
    parent->XJAddChild(child);

    parent->AddComponent<XJ::XJScriptComponent>()
        .AddSlot({first, XJ::XJAssetType::Script});
    child->AddComponent<XJ::XJScriptComponent>()
        .AddSlot({first, XJ::XJAssetType::Script});
    system.SynchronizeSlots();

    scene.DestroyEntity(parent);
    Check(invoker.DestroyCalls == 8, "recursive deletion cleans both entities");

    auto* bulk = scene.CreateEntityWithTransform("Bulk");
    bulk->AddComponent<XJ::XJScriptComponent>()
        .AddSlot({first, XJ::XJAssetType::Script});
    system.SynchronizeSlots();

    scene.DestroyAllEntity();
    Check(invoker.DestroyCalls == 9, "bulk deletion runs cleanup");

    system.OnDestroy();
    system.OnDestroy();
    Check(invoker.DestroyCalls == 9, "stop never repeats completed cleanup");
}

void TestLifecycleEdges(XJ::XJAssetRegistry& registry)
{
    // 清理错误不能阻止实例终止，也不能在 Stop 时再次执行清理。
    {
        XJ::XJScene scene;
        RecordingInvoker invoker;
        auto* owner = scene.CreateEntityWithTransform("CleanupFailure");
        auto& component = owner->AddComponent<XJ::XJScriptComponent>();
        component.AddSlot({9001, XJ::XJAssetType::Script});
        XJ::XJScriptSystem system(scene, registry, {}, {}, &invoker);
        system.OnCreate();
        invoker.Callback = [](XJ::XJScriptNativeContext&, double angle)
        {
            if (angle == 4.0)
                throw std::runtime_error("cleanup native failure");
        };
        component.ClearSlots();
        system.SynchronizeSlots();
        system.OnDestroy();
        Check(invoker.DestroyCalls == 1, "failed OnDestroy is attempted only once");
    }

    // 销毁通知中的批量销毁必须延迟，避免重入导致无限循环。
    {
        XJ::XJScene scene;
        RecordingInvoker invoker;
        auto* first = scene.CreateEntityWithTransform("FirstDestroyed");
        auto* second = scene.CreateEntityWithTransform("SecondDestroyed");
        first->AddComponent<XJ::XJScriptComponent>()
            .AddSlot({9001, XJ::XJAssetType::Script});
        second->AddComponent<XJ::XJScriptComponent>()
            .AddSlot({9001, XJ::XJAssetType::Script});
        XJ::XJScriptSystem system(scene, registry, {}, {}, &invoker);
        system.OnCreate();
        invoker.Callback = [&](XJ::XJScriptNativeContext& context, double angle)
        {
            if (angle == 4.0)
            {
                scene.DestroyAllEntity();
                Check(context.Owner->IsValid(), "cleanup owner survives reentrant bulk delete");
            }
        };
        scene.DestroyEntity(first);
        Check(scene.GetEntities().size() == 1, "bulk cleanup request stays queued");
        scene.FlushDestroyQueue();
        Check(scene.GetEntities().empty() && invoker.DestroyCalls == 2,
              "reentrant bulk cleanup finishes exactly once for both owners");
    }

    // OnCreate 失败会清理已进入生命周期的实例，且不会每帧重试。
    {
        XJ::XJScene scene;
        RecordingInvoker invoker;
        auto* owner = scene.CreateEntityWithTransform("CreationFailure");
        auto& component = owner->AddComponent<XJ::XJScriptComponent>();
        XJ::XJScriptSystem system(scene, registry, {}, {}, &invoker);
        system.OnCreate();
        const auto slotId = component.AddSlot({9005, XJ::XJAssetType::Script})->SlotId;
        system.SynchronizeSlots();
        Check(invoker.DestroyCalls == 1, "failed OnCreate attempts cleanup");
        system.SynchronizeSlots();
        Check(invoker.DestroyCalls == 1, "unchanged failed slot is not retried");
        component.SetSlotScript(slotId, {9001, XJ::XJAssetType::Script});
        system.SynchronizeSlots();
        system.OnDestroy();
        Check(invoker.DestroyCalls == 2, "failed slot can be replaced successfully");
    }

    // Native 中删除 Owner 会延迟到安全点，不能使当前 VM 的 Owner 失效。
    {
        XJ::XJScene scene;
        RecordingInvoker invoker;
        auto* owner = scene.CreateEntityWithTransform("SelfDestroy");
        const auto ownerId = owner->XJGetUUID();
        owner->AddComponent<XJ::XJScriptComponent>()
            .AddSlot({9001, XJ::XJAssetType::Script});
        invoker.Callback = [&](XJ::XJScriptNativeContext& context, double angle)
        {
            if (angle == 1.0)
            {
                scene.DestroyEntity(context.Owner);
                scene.FlushDestroyQueue();
                Check(context.Owner->IsValid(), "owner survives the native call");
            }
        };
        XJ::XJScriptSystem system(scene, registry, {}, {}, &invoker);
        system.OnCreate();
        Check(scene.FindEntityByUUID(ownerId) != nullptr, "self destruction stays queued");
        scene.FlushDestroyQueue();
        Check(!scene.FindEntityByUUID(ownerId) && invoker.DestroyCalls == 1,
              "self destruction cleans exactly once at safe point");
    }

    // OnCreate 修改 slot vector 不得使同步过程的 iterator 失效。
    {
        XJ::XJScene scene;
        RecordingInvoker invoker;
        auto* owner = scene.CreateEntityWithTransform("ComponentMutation");
        auto& component = owner->AddComponent<XJ::XJScriptComponent>();
        component.AddSlot({9001, XJ::XJAssetType::Script});
        component.AddSlot({9001, XJ::XJAssetType::Script});
        invoker.Callback = [](XJ::XJScriptNativeContext& context, double angle)
        {
            if (angle == 1.0)
                context.Owner->GetComponent<XJ::XJScriptComponent>().ClearSlots();
        };
        XJ::XJScriptSystem system(scene, registry, {}, {}, &invoker);
        system.OnCreate();
        system.SynchronizeSlots();
        Check(invoker.DestroyCalls == 1, "slot mutation cleans only the created instance");
    }

    // 启动失败回滚，以及重复 OnCreate 不得重复调度。
    {
        XJ::XJScene scene;
        RecordingInvoker invoker;
        scene.CreateEntityWithUUIDAndTransform(XJ::XJUUID{1}, "First")
            ->AddComponent<XJ::XJScriptComponent>()
            .AddSlot({9001, XJ::XJAssetType::Script});
        scene.CreateEntityWithUUIDAndTransform(XJ::XJUUID{2}, "Failing")
            ->AddComponent<XJ::XJScriptComponent>()
            .AddSlot({9005, XJ::XJAssetType::Script});
        XJ::XJScriptSystem system(scene, registry, {}, {}, &invoker);
        bool rejected = false;
        try { system.OnCreate(); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && invoker.DestroyCalls == 2, "startup failure rolls back both instances");
        system.OnDestroy();
        Check(invoker.DestroyCalls == 2, "rollback cleanup is not repeated");
    }

    // Scene 可以先销毁；System 后续停止不得访问已销毁的 signal。
    {
        RecordingInvoker invoker;
        auto scene = std::make_unique<XJ::XJScene>();
        scene->CreateEntityWithTransform("SceneLifetime")
            ->AddComponent<XJ::XJScriptComponent>()
            .AddSlot({9001, XJ::XJAssetType::Script});
        XJ::XJScriptSystem system(*scene, registry, {}, {}, &invoker);
        system.OnCreate();
        bool rejected = false;
        try { system.OnCreate(); }
        catch (const std::logic_error&) { rejected = true; }
        Check(rejected, "repeated creation is rejected");
        scene.reset();
        system.OnDestroy();
        Check(invoker.DestroyCalls == 1 && invoker.DestroyOwnersValid,
              "scene destruction cleans while owner is valid");
    }
}
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "XJScriptSystemTests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    const auto scriptPath = root / "Lifecycle.xjs";
    std::ofstream output(scriptPath, std::ios::binary | std::ios::trunc);
    output <<
        "class Lifecycle : ScriptBehaviour { public: "
        "[[FieldId(\"1001\")]] float speed = 10.0f; "
        "void OnCreate() { Transform.RotateY(1.0f); } "
        "void OnUpdate(float dt) { Transform.RotateY(speed * dt); } "
        "void OnFixedUpdate(float dt) { Transform.RotateY(2.0f); } "
        "void OnDestroy() { Transform.RotateY(4.0f); } }";
    output.close();

    const auto replacementPath = root / "Replacement.xjs";
    {
        std::ofstream file(replacementPath);
        file <<
            "class Replacement : ScriptBehaviour { public: "
            "void OnCreate() { Transform.RotateY(20.0f); } "
            "void OnDestroy() { Transform.RotateY(40.0f); } }";
        Check(file.good(), "replacement script written");
    }

    const auto faultPath = root / "Faulted.xjs";
    {
        std::ofstream file(faultPath);
        file <<
            "class Faulted : ScriptBehaviour { public: "
            "void OnUpdate(float dt) { int zero = 0; int value = 1 / zero; } "
            "void OnDestroy() { Transform.RotateY(4.0f); } }";
        Check(file.good(), "faulted script written");
    }

    const auto creationFaultPath = root / "CreationFault.xjs";
    {
        std::ofstream file(creationFaultPath);
        file <<
            "class CreationFault : ScriptBehaviour { public: "
            "void OnCreate() { int zero = 0; int value = 1 / zero; } "
            "void OnDestroy() { Transform.RotateY(4.0f); } }";
        Check(file.good(), "creation fault script written");
    }

    XJ::XJAssetRegistry registry;
    Check(registry.RegisterAsset({9001, XJ::XJAssetType::Script, "Lifecycle", scriptPath, {}}),
          "script registered");

          Check(registry.RegisterAsset({
          9003, XJ::XJAssetType::Script,
          "Replacement", replacementPath, {}}),
      "replacement script registered");

Check(registry.RegisterAsset({
          9004, XJ::XJAssetType::Script,
          "Faulted", faultPath, {}}),
      "faulted script registered");

    Check(registry.RegisterAsset({9005, XJ::XJAssetType::Script,
                                "CreationFault", creationFaultPath, {}}),
          "creation fault script registered");

    XJ::XJScene scene;
    XJ::XJEntity* entity = scene.CreateEntityWithTransform("ScriptEntity");
    auto& scripts = entity->AddComponent<XJ::XJScriptComponent>();
    auto* slot = scripts.AddSlotWithId(
        XJ::XJUUID{9002}, {9001, XJ::XJAssetType::Script}, true);
    Check(slot != nullptr, "script slot added");
    Check(scripts.SetFieldOverride(XJ::XJUUID{9002}, 1001, XJ::XJScriptValue{20.0}),
          "speed override set");

    XJ::XJScriptSystem system(scene, registry);
    const uint64_t statisticsFrame = uint64_t{1} << 40;
    system.BeginStatisticsFrame(statisticsFrame);
    system.OnCreate();
    const auto creationStatistics = system.GetStatistics();
    Check(creationStatistics.FrameId == statisticsFrame &&
          creationStatistics.ActiveInstances == 1 && creationStatistics.CpuMilliseconds > 0.0,
          "real OnCreate publishes timed statistics and active instances");
    system.BeginStatisticsFrame(statisticsFrame + 1);
    system.OnUpdate(0.5f);
    system.OnFixedUpdate(1.0f / 60.0f);
    system.OnDestroy();

    const auto stoppedStatistics = system.GetStatistics();
    Check(stoppedStatistics.FrameId == statisticsFrame + 1 &&
          stoppedStatistics.ActiveInstances == 0 && stoppedStatistics.CpuMilliseconds > 0.0,
          "update and stop preserve the completed statistics frame");

    const auto& transform = entity->GetComponent<XJ::XJTransformComponent>();
    Check(std::abs(transform.rotation.y - 17.0f) < 0.0001f,
          "all script lifecycle methods execute with override");

    TestDynamicLifecycle(registry, 9001, 9003, 9004);
    TestLifecycleEdges(registry);

    std::filesystem::remove_all(root, ec);
    spdlog::info("XJScriptSystem tests passed.");

    return EXIT_SUCCESS;
}
