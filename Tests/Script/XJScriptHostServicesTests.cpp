#include "Asset/Importer/XJScriptAssetCompiler.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/XJScriptAsset.h"
#include "ECS/Component/XJScriptComponent.h"
#include "ECS/System/XJScriptSystem.h"
#include "Script/Host/XJEcsScriptTransformService.h"
#include "Script/Host/XJScriptInputSnapshotService.h"
#include "Script/Host/XJSpdlogScriptLogService.h"
#include "Script/Runtime/XJScriptHostNativeInvoker.h"
#include "Script/Runtime/XJScriptRuntime.h"

#include <spdlog/sinks/base_sink.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <string_view>

namespace
{
    using namespace XJ;
    using Space = XJScriptSpace;
    using Property = XJScriptTransformProperty;
    using Id = XJScriptNativeFunctionId;

    void Check(bool value, std::string_view message)
    {
        if (!value)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    bool Near(XJScriptVec3 a, XJScriptVec3 b, double tolerance = 1.0e-4)
    {
        return std::abs(a.X - b.X) < tolerance &&
               std::abs(a.Y - b.Y) < tolerance &&
               std::abs(a.Z - b.Z) < tolerance;
    }

    struct Fixture
    {
        XJScene Scene;
        std::shared_ptr<XJEcsScriptEntityService> Entities =
            std::make_shared<XJEcsScriptEntityService>(Scene);
        std::shared_ptr<XJEcsScriptTransformService> Transforms =
            std::make_shared<XJEcsScriptTransformService>(Entities);
        std::shared_ptr<XJScriptInputSnapshotService> Input =
            std::make_shared<XJScriptInputSnapshotService>();
        std::shared_ptr<XJSpdlogScriptLogService> Log =
            std::make_shared<XJSpdlogScriptLogService>();
        XJEntity* Owner = Scene.CreateEntityWithTransform("Owner");
        XJScriptEntityRef Reference = Entities->Capture(*Owner);
        XJScriptHostNativeInvoker Invoker{{Entities, Transforms, Input, Log}};
        XJScriptNativeContext Context{Owner, {Reference, 19, uint64_t{1} << 40, XJScriptPhase::Update}};
    };

    void TestEntities()
    {
        Fixture fixture;
        auto& entities = *fixture.Entities;
        Check(entities.Capture(*fixture.Owner) == fixture.Reference, "capture is stable");
        const auto uuid = entities.GetUUID(fixture.Reference);
        Check(uuid.IsValid(), "UUID can be read");
        Check(entities.FindByUUID(*uuid.Value).Value == fixture.Reference, "decimal lookup resolves identity");
        auto* hexEntity = fixture.Scene.CreateEntityWithUUIDAndTransform(XJUUID{0xabcd}, "Hex");
        Check(entities.FindByUUID("0xABCD").Value == entities.Capture(*hexEntity), "hexadecimal UUID lookup");
        Check(entities.FindByUUID("123456789").Value->IsNull(), "missing UUID returns null");
        for (const auto* invalid : {"", "0", "-1", "+1", " 1", "1x", "0x", "18446744073709551616"})
            Check(!entities.FindByUUID(invalid).IsValid(), "malformed UUID rejected");
        Check(!entities.IsValid({}), "null is invalid");
        Check(!entities.IsValid({fixture.Reference.ScopeId, fixture.Reference.Index, 2}), "wrong generation rejected");
        XJEcsScriptEntityService otherSession(fixture.Scene);
        Check(!otherSession.IsValid(fixture.Reference), "references cannot cross sessions");
        XJScene otherScene;
        Check(entities.Capture(*otherScene.CreateEntityWithTransform("Foreign")).IsNull(), "foreign entity rejected");

        // 即使外部仍持有旧 wrapper，也不能在相同 UUID 重建时复活旧句柄。
        const auto held = entities.Resolve(fixture.Reference);
        const XJUUID id = fixture.Owner->XJGetUUID();
        fixture.Scene.DestroyEntity(fixture.Owner);
        Check(!entities.IsValid(fixture.Reference), "destroyed wrapper invalidates reference");
        auto* replacement = fixture.Scene.CreateEntityWithUUIDAndTransform(id, "Replacement");
        Check(entities.Capture(*replacement) != fixture.Reference &&
              !entities.IsValid(fixture.Reference), "same UUID does not revive old reference");

        auto scene = std::make_unique<XJScene>();
        XJEcsScriptEntityService service(*scene);
        const auto reference = service.Capture(*scene->CreateEntityWithTransform("Temporary"));
        scene.reset();
        Check(!service.IsValid(reference) && !service.FindByUUID("1").IsValid(), "scene lifetime protects service");
    }

    void TestTransforms()
    {
        Fixture fixture;
        auto& service = *fixture.Transforms;
        const auto ref = fixture.Reference;
        auto& local = fixture.Owner->GetComponent<XJTransformComponent>();
        Check(service.Write(ref, Space::Local, Property::Position, {1, 2, 3}).IsValid(), "local position writes");
        Check(Near(*service.Read(ref, Space::Local, Property::Position).Value, {1, 2, 3}), "local position reads");
        Check(service.Write(ref, Space::Local, Property::EulerAngles, {17, 31, -29}).IsValid(), "Euler write");
        const glm::mat4 candidate = local.GetLocalMatrix();
        const glm::mat4 expectedLocal = glm::translate(glm::mat4{1}, glm::vec3{1, 2, 3}) *
            glm::eulerAngleXYZ(glm::radians(17.0f), glm::radians(31.0f), glm::radians(-29.0f));
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                Check(std::abs(candidate[column][row] - expectedLocal[column][row]) < 1.0e-5f,
                      "service matrix agrees with engine Rx Ry Rz convention");

        auto* parent = fixture.Scene.CreateEntityWithTransform("Parent");
        parent->XJAddChild(fixture.Owner);
        auto& p = parent->GetComponent<XJTransformComponent>();
        Check(p.SetLocalTRSEuler({10, 20, 30}, {15, 30, -20}, {2, 3, 4}) == XJTransformStatus::Success,
              "parent setup");
        const auto expected = p.GetLocalMatrix() * local.GetLocalMatrix();
        Check(Near(*service.Read(ref, Space::World, Property::Position).Value,
                   {expected[3].x, expected[3].y, expected[3].z}), "parent matrix propagates to world position");

        Check(service.Write(ref, Space::World, Property::Position, {40, -5, 9}).IsValid(), "world position writes");
        Check(Near(*service.Read(ref, Space::World, Property::Position).Value, {40, -5, 9}), "world position read after write");
        Check(service.Write(ref, Space::World, Property::EulerAngles, {20, 35, -15}).IsValid(), "world rotation writes");
        Check(Near(*service.Read(ref, Space::World, Property::EulerAngles).Value, {20, 35, -15}), "world rotation read after write");
        Check(local.SetLocalScale({-1, 1, 1}) == XJTransformStatus::Success, "local reflection setup");
        Check(service.Write(ref, Space::World, Property::Scale, {5, 6, 7}).IsValid(), "lossy world scale writes with shear");
        Check(local.GetLocalScale().x < 0 && Near(*service.Read(ref, Space::World, Property::Scale).Value, {5, 6, 7}),
              "scale axes hit target while preserving reflection");

        const auto oldPosition = local.GetLocalPosition();
        const auto oldRotation = local.GetLocalRotation();
        const auto oldScale = local.GetLocalScale();
        const auto oldMatrix = local.GetLocalMatrix();
        Check(p.SetLocalScale({0, 0, 0}) == XJTransformStatus::Success, "singular parent setup");
        Check(!service.Write(ref, Space::World, Property::Position, {1, 2, 3}).IsValid(), "singular parent rejects world position");
        Check(!service.Write(ref, Space::World, Property::Scale, {1, 2, 3}).IsValid(), "collapsed axes reject world scale");
        Check(!service.Write(ref, Space::World, Property::Scale, {-1, 2, 3}).IsValid(), "negative world scale rejected");
        Check(!service.Write(ref, Space::Local, Property::Position,
                             {std::numeric_limits<double>::infinity(), 0, 0}).IsValid(), "non-finite write rejected");
        Check(!service.Write(ref, Space::Local, Property::Position, {1.0e300, 0, 0}).IsValid(), "float storage overflow rejected");
        Check(!service.Write(ref, Space::Local, Property::Scale, {1.0e-300, 1, 1}).IsValid(),
              "float underflow cannot silently collapse an axis");
        Check(local.GetLocalPosition() == oldPosition && local.GetLocalRotation() == oldRotation &&
              local.GetLocalScale() == oldScale && local.GetLocalMatrix() == oldMatrix, "failed writes preserve all transform state");
        Check(!service.Read(ref, static_cast<Space>(99), Property::Position).IsValid(), "invalid space rejected");
        Check(!service.Write(ref, static_cast<Space>(99), Property::Position, {}).IsValid(), "invalid write space rejected");
        Check(!service.Read(ref, Space::Local, static_cast<Property>(99)).IsValid(), "invalid property rejected");
        Check(!service.Read({}, Space::Local, Property::Position).IsValid(), "null reference rejected");
        auto* noTransform = fixture.Scene.CreateEntity("NoTransform");
        Check(!service.Read(fixture.Entities->Capture(*noTransform), Space::Local, Property::Position).IsValid(),
              "missing Transform rejected");
        // 模拟非法底层 parent setter，读完立即恢复，避免影响 Scene 清理。
        XJNode* originalParent = parent->XJGetParent();
        parent->XJSetParent(fixture.Owner);
        const auto cycle = service.Read(ref, Space::World, Property::Position);
        parent->XJSetParent(originalParent);
        Check(!cycle.IsValid(), "hierarchy cycles rejected");
        bool rejectedNull = false;
        try { XJEcsScriptTransformService invalid(nullptr); }
        catch (const std::invalid_argument&) { rejectedNull = true; }
        Check(rejectedNull, "missing service dependency rejected at construction");

        Fixture huge;
        XJEntity* child = huge.Owner;
        for (int level = 0; level < 8; ++level)
        {
            auto* ancestor = huge.Scene.CreateEntityWithTransform("LargeScale");
            Check(ancestor->GetComponent<XJTransformComponent>().SetLocalScale({1.0e30, 1.0e30, 1.0e30}) ==
                  XJTransformStatus::Success, "large ancestor scale");
            ancestor->XJAddChild(child);
            child = ancestor;
        }
        const auto largeScale = huge.Transforms->Read(huge.Reference, Space::World, Property::Scale);
        Check(largeScale.IsValid() && largeScale.Value->X > 1.0e230, "finite axis lengths do not overflow intermediate squares");
        Check(huge.Transforms->Write(huge.Reference, Space::World, Property::Scale, *largeScale.Value).IsValid(),
              "large but finite world scale can round trip");
    }

    void TestInput()
    {
        XJScriptInputSnapshotService service;
        XJScriptInputSnapshotService::KeyboardSnapshot input;
        input.Held[KEY_D] = true;
        input.Held[KEY_W] = true;
        input.Held[KEY_SPACE] = true;
        input.Pressed[KEY_SPACE] = true;
        service.PublishKeyboard(input, true);
        Check(*service.IsKeyHeld("Space").Value && *service.GetAxis("Horizontal").Value == 1.0 &&
              *service.GetAxis("Vertical").Value == 1.0, "held keys and axes survive focus acquisition");
        Check(!*service.WasKeyPressed("Space", XJScriptPhase::Update).Value, "focus acquisition suppresses edges");
        service.PublishKeyboard(input, true);
        Check(*service.WasKeyPressed("Space", XJScriptPhase::Update).Value &&
              *service.WasKeyPressed("Space", XJScriptPhase::Update).Value, "edges are shared not consumed");
        Check(!*service.WasKeyPressed("Space", XJScriptPhase::FixedUpdate).Value &&
              !*service.WasKeyPressed("Space", XJScriptPhase::Create).Value &&
              !*service.WasKeyPressed("Space", XJScriptPhase::Destroy).Value, "edges limited to Update");
        input.Held[KEY_A] = true;
        input.Held[KEY_SPACE] = false;
        input.Pressed[KEY_SPACE] = false;
        input.Released[KEY_SPACE] = true;
        service.PublishKeyboard(input, true);
        Check(*service.GetAxis("Horizontal").Value == 0.0 &&
              *service.WasKeyReleased("Space", XJScriptPhase::Update).Value, "opposite keys cancel and release edge works");
        service.PublishKeyboard(input, false);
        Check(*service.GetAxis("Vertical").Value == 0.0 && !*service.IsKeyHeld("W").Value &&
              !*service.WasKeyReleased("Space", XJScriptPhase::Update).Value, "blocked input is neutral");
        service.PublishKeyboard(input, true);
        Check(!*service.WasKeyReleased("Space", XJScriptPhase::Update).Value, "resume suppresses stale release");
        service.Reset();
        service.Publish(XJInputState{}, true);
        Check(!*service.IsKeyHeld("Space").Value, "platform state publication replaces snapshot");
        Check(!service.GetAxis("Unknown").IsValid() && !service.IsKeyHeld("space").IsValid(), "unknown input names rejected");
    }

    class MemorySink final : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
        std::vector<std::string> Messages;
        std::vector<spdlog::level::level_enum> Levels;
    private:
        void sink_it_(const spdlog::details::log_msg& message) override
        {
            Messages.emplace_back(message.payload.data(), message.payload.size());
            Levels.push_back(message.level);
        }
        void flush_() override {}
    };

    void TestLogging()
    {
        const auto previous = spdlog::default_logger();
        auto sink = std::make_shared<MemorySink>();
        auto logger = std::make_shared<spdlog::logger>("ScriptHostTests", sink);
        logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger);
        XJSpdlogScriptLogService service;
        XJScriptCallContext context{{7, 0, 1}, 99, uint64_t{1} << 40, XJScriptPhase::Update};
        Check(service.Write(context, XJScriptLogLevel::Info, "braces {} are data").IsValid(), "info logs");
        service.Write(context, XJScriptLogLevel::Warning, "warning");
        service.Write(context, XJScriptLogLevel::Error, std::string(6000, 'x'));
        Check(sink->Messages.size() == 3 && sink->Levels[0] == spdlog::level::info &&
              sink->Levels[1] == spdlog::level::warn && sink->Levels[2] == spdlog::level::err,
              "log levels route to spdlog");
        Check(sink->Messages[0].find("frame=1099511627776") != std::string::npos &&
              sink->Messages[0].find("braces {} are data") != std::string::npos &&
              sink->Messages[2].size() < 4300, "context preserves 64-bit frame and messages are bounded");
        for (int index = 0; index < 17; ++index)
            service.Write(context, XJScriptLogLevel::Info, "burst");
        Check(sink->Messages.size() == 16 && service.GetDroppedCount() == 4, "per-slot quota drops without failure");
        for (uint64_t slot = 100; slot < 400; ++slot)
        {
            context.SlotId = slot;
            Check(service.Write(context, XJScriptLogLevel::Info, "global burst").IsValid(), "quota does not fault script");
        }
        Check(sink->Messages.size() == 256, "global quota is bounded");
        Check(!service.Write(context, static_cast<XJScriptLogLevel>(99), "invalid").IsValid(), "invalid log level rejected");
        spdlog::set_default_logger(previous);
    }

    void TestDispatcherAndVM()
    {
        Fixture fixture;
        const std::array<XJScriptValue, 1> rotationStep{15.0};
        for (int index = 0; index < 31; ++index)
            Check(fixture.Invoker.Invoke(Id::TransformRotateY, rotationStep, fixture.Context).IsValid(),
                  "host multi-turn RotateY succeeds");
        const auto expectedRotation = XJTransformComponent::RotationFromEulerXYZ({0, 465, 0});
        const auto actualRotation = glm::normalize(glm::dquat{
            fixture.Owner->GetComponent<XJTransformComponent>().GetLocalRotation()});
        Check(std::abs(glm::dot(expectedRotation, actualRotation)) > 0.99999,
              "host RotateY does not reverse at Euler singularities");
        const auto catalogue = fixture.Invoker.GetNativeCatalogue();
        const char* names[] = {
            "GetLocalPosition", "SetLocalPosition", "GetWorldPosition", "TrySetWorldPosition",
            "GetLocalEulerAngles", "SetLocalEulerAngles", "GetWorldEulerAngles", "TrySetWorldEulerAngles",
            "GetLocalScale", "SetLocalScale", "GetLossyWorldScale", "TrySetWorldScale"
        };
        for (int operation = 0; operation < 12; ++operation)
        {
            for (int explicitEntity = 0; explicitEntity < 2; ++explicitEntity)
            {
                std::vector<XJScriptValueType> types;
                std::vector<XJScriptValue> values;
                if (explicitEntity)
                {
                    types.push_back(XJScriptValueType::EntityRef);
                    values.emplace_back(fixture.Reference);
                }
                if (operation % 2)
                {
                    types.push_back(XJScriptValueType::Vec3);
                    values.emplace_back(XJScriptVec3{2, 3, 4});
                }
                const auto* descriptor = catalogue->Resolve("Transform", names[operation], types);
                Check(descriptor != nullptr, "all 24 Transform signatures resolve");
                const auto result = fixture.Invoker.Invoke(descriptor->Id, values, fixture.Context);
                Check(result.IsValid(), "all Transform overloads dispatch");
                if (descriptor->ReturnType == XJScriptValueType::Void)
                    Check(!result.ReturnValue, "local setter returns void");
                else
                    Check(result.ReturnValue && XJGetScriptValueType(*result.ReturnValue) == descriptor->ReturnType,
                          "dispatcher return type matches catalogue");
            }
        }
        Check(!fixture.Invoker.Invoke(static_cast<Id>(9999), {}, fixture.Context).IsValid(), "unknown native rejected");
        Check(!fixture.Invoker.Invoke(Id::InputGetAxis, {}, fixture.Context).IsValid(), "bad arity rejected");
        const std::array<XJScriptValue, 1> wrong{int64_t{1}};
        Check(!fixture.Invoker.Invoke(Id::InputGetAxis, wrong, fixture.Context).IsValid(), "bad type rejected");
        XJScriptHostNativeInvoker missingServices({});
        Check(!missingServices.Invoke(Id::EntitySelf, {}, fixture.Context).IsValid(), "unavailable service reported");

        const auto asset = XJScriptAssetCompiler::CompileSource(R"(
            class HostTest : ScriptBehaviour {
            public:
                bool Run() {
                    EntityRef self = Entity.Self();
                    EntityRef found = Scene.FindByUUID(Entity.GetUUID(self));
                    Transform.SetLocalPosition(found, Vec3(2.0f, 3.0f, 4.0f));
                    Vec3 position = Transform.GetLocalPosition();
                    float axis = Input.GetAxis("Horizontal");
                    bool pressed = Input.WasKeyPressed("Space");
                    Log.Info("VM host integration");
                    return self == found && Entity.IsValid(found) && position.y == 3.0f && axis == 0.0f && !pressed;
                }
            }
        )", "HostTest.xjs");
        Check(asset && asset->IsCompiled(), "new host API compiles end to end");
        XJScriptInstance instance;
        Check(!XJScriptRuntime::InitializeInstance(instance, asset->Module), "host VM instance initializes");
        const auto result = XJScriptRuntime::Execute(instance, 0, {}, fixture.Invoker, fixture.Context);
        Check(result.IsValid() && result.ReturnValue && std::get<bool>(*result.ReturnValue), "host API executes through VM");

        auto* parent = fixture.Scene.CreateEntityWithTransform("Singular");
        parent->XJAddChild(fixture.Owner);
        Check(parent->GetComponent<XJTransformComponent>().SetLocalScale({0, 0, 0}) ==
              XJTransformStatus::Success, "singular dispatcher parent");
        const auto* setter = catalogue->Resolve("Transform", "TrySetWorldPosition",
            std::array{XJScriptValueType::Vec3});
        const std::array<XJScriptValue, 1> position{XJScriptVec3{1, 2, 3}};
        const auto failedTry = fixture.Invoker.Invoke(setter->Id, position, fixture.Context);
        Check(failedTry.IsValid() && !std::get<bool>(*failedTry.ReturnValue), "mathematical failure returns false");
        fixture.Context.Call.Owner = {};
        Check(!fixture.Invoker.Invoke(setter->Id, position, fixture.Context).IsValid(), "missing owner is not swallowed by TrySet");
    }

    void TestSystemLifecycle()
    {
        const auto root = std::filesystem::temp_directory_path() / "opencode" /
            ("XJScriptHostServicesTests-" + std::to_string(static_cast<uint64_t>(XJUUID{})));
        std::filesystem::create_directories(root);
        const auto path = root / "Lifecycle.xjs";
        {
            std::ofstream file(path);
            file << R"(
                class Lifecycle : ScriptBehaviour {
                private: EntityRef self = null;
                public:
                    void OnCreate() {
                        self = Entity.Self();
                        Transform.SetLocalPosition(Vec3(1.0f, 2.0f, 3.0f));
                    }
                    void OnUpdate(float dt) {
                        Vec3 value = Transform.GetLocalPosition(self);
                        value.y += dt;
                        Transform.SetLocalPosition(self, value);
                    }
                    void OnDestroy() {
                        if (Entity.IsValid(self)) {
                            Vec3 value = Transform.GetLocalPosition(self);
                            value.x += 10.0f;
                            Transform.SetLocalPosition(self, value);
                            Log.Info("destroy owner remains valid");
                        }
                    }
                }
            )";
            Check(file.good(), "lifecycle fixture written");
        }
        XJAssetRegistry registry;
        Check(registry.RegisterAsset({8001, XJAssetType::Script, "Lifecycle", path, {}}), "fixture registered");
        XJScene scene;
        auto* owner = scene.CreateEntityWithTransform("LifecycleOwner");
        auto& component = owner->AddComponent<XJScriptComponent>();
        const XJUUID slotId = component.AddSlot({8001, XJAssetType::Script})->SlotId;
        XJScriptSystem system(scene, registry);
        system.BeginStatisticsFrame(uint64_t{1} << 40);
        system.OnCreate();
        system.PublishInput({}, true);
        system.OnUpdate(0.5f);
        Check(owner->GetComponent<XJTransformComponent>().GetLocalPosition().y == 2.5f, "default host runs new APIs during Update");
        component.SetSlotEnabled(slotId, false);
        system.SynchronizeSlots();
        Check(owner->GetComponent<XJTransformComponent>().GetLocalPosition().x == 11.0f, "disable cleanup retains entity service");
        component.SetSlotEnabled(slotId, true);
        system.SynchronizeSlots();
        system.OnDestroy();
        Check(owner->GetComponent<XJTransformComponent>().GetLocalPosition().x == 11.0f, "Stop cleans before releasing host");
        system.OnCreate();
        system.OnUpdate(0.25f);
        Check(system.GetStatistics().ActiveInstances == 1 && owner->GetComponent<XJTransformComponent>().GetLocalPosition().y == 2.25f,
              "restart rebuilds default host without dangling Invoker");
        system.OnDestroy();
        std::filesystem::remove(path);
        std::filesystem::remove(root);
    }

    void TestResourceScripts()
    {
        const std::filesystem::path root = XJ_TEST_SOURCE_DIR;
        for (const auto* name : {"Rotate", "PlayerMove", "MoveTarget", "WorldPosition", "WorldScale"})
        {
            const auto asset = XJScriptAssetCompiler::CompileFile(root / "Resource" / "Script" / (std::string(name) + ".xjs"));
            if (asset && !asset->IsCompiled())
                for (const auto& diagnostic : asset->Diagnostics)
                    spdlog::error("{}: {}:{} {}", name, diagnostic.Line, diagnostic.Column, diagnostic.Message);
            Check(asset && asset->IsCompiled() && asset->Module->ClassName == name, "resource script class matches its file");
        }
    }
}

int main()
{
    TestEntities();
    TestTransforms();
    TestInput();
    TestLogging();
    TestDispatcherAndVM();
    TestSystemLifecycle();
    TestResourceScripts();
    spdlog::info("Script host service tests passed.");
    return EXIT_SUCCESS;
}
