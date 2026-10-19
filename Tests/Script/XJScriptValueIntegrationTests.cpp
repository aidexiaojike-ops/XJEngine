#include "Asset/Importer/XJScriptAssetCompiler.h"
#include "Asset/XJScriptAsset.h"
#include "Script/Runtime/XJScriptRuntime.h"
#include "Script/Serialization/XJScriptValueCodec.h"
#include "Script/Bytecode/XJScriptBytecodeVerifier.h"
#include "Script/Compiler/XJScriptCompiler.h"
#include "Script/Language/XJScriptParser.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include "ECS/Component/XJScriptComponent.h"

#include <spdlog/spdlog.h>
#include <array>
#include <cstdlib>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <limits>

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

    class Host final : public XJ::XJScriptNativeInvoker
    {
    public:
        XJ::XJScriptNativeInvokeResult Invoke(
            XJ::XJScriptNativeFunctionId,
            std::span<const XJ::XJScriptValue>,
            XJ::XJScriptNativeContext&) override
        {
            return XJ::XJScriptNativeInvokeResult::Failure(
                XJ::XJScriptNativeErrorCode::UnsupportedFunction,
                "Test host has no ECS operations");
        }
    };

    uint32_t FindFunction(
        const XJ::XJScriptBytecodeModule& module,
        std::string_view name)
    {
        for (uint32_t index = 0; index < module.Functions.size(); ++index)
        {
            if (module.Functions[index].Name == name)
                return index;
        }

        Check(false, "function exists");
        return 0;
    }

    std::shared_ptr<const XJ::XJScriptBytecodeModule> Compile(std::string_view body)
    {
        const auto asset = XJ::XJScriptAssetCompiler::CompileSource(
            "class Test : ScriptBehaviour { " + std::string(body) + " }", "Test.xjs");
        if (asset && !asset->IsCompiled())
            for (const auto& diagnostic : asset->Diagnostics)
                spdlog::error("{}:{} {}", diagnostic.Line, diagnostic.Column, diagnostic.Message);
        Check(asset && asset->IsCompiled(), "test body compiles");
        return asset->Module;
    }

    void TestErrorsAndValues()
    {
        const char* rejected[] = {
            "public: void Run() { Vec3(1.0f, 2.0f, 3.0f).x = 4.0f; }",
            "public: void Run() { ++Vec3(1.0f, 2.0f, 3.0f).x; }",
            "public: void Run() { Vec3 v = Vec3(1, 2, 3); }",
            "public: [[FieldId(\"1\")]] EntityRef reference = null;",
            "public: void Run() { Vec3 v = Vec3(1.0f, 2.0f, 3.0f); float n = v.w; }",
            "public: void Run() { Vec3 v = Vec3(1.0f, 2.0f, 3.0f); v *= v; }"
        };
        for (const auto* body : rejected)
        {
            const auto asset = XJ::XJScriptAssetCompiler::CompileSource(
                "class Test : ScriptBehaviour { " + std::string(body) + " }", "Test.xjs");
            Check(asset && !asset->IsCompiled(), "invalid member/type expressions are rejected");
        }

        const auto module = Compile(R"(
            private: Vec3 field = Vec3(1.0f, 2.0f, 3.0f);
            public:
            float Run(Vec3 parameter)
            {
                Vec3 local = Vec3(1.0f, 2.0f, 3.0f);
                float a = field.x++;
                float b = ++parameter.y;
                float c = local.z--;
                float d = --local.x;
                return a * 1000 + b * 100 + c * 10 + d;
            }
            Vec3 Math()
            {
                Vec3(0.0f, 0.0f, 0.0f);
                Vec3 v = -Vec3(1.0f, 2.0f, 3.0f);
                v += Vec3(3.0f, 4.0f, 5.0f);
                v *= 2;
                v /= 2;
                v -= Vec3(1.0f, 1.0f, 1.0f);
                return 2 * v;
            }
        )");
        XJ::XJScriptInstance instance;
        Check(!XJ::XJScriptRuntime::InitializeInstance(instance, module), "extended value module initializes");
        Host host;
        XJ::XJScriptNativeContext context;
        const std::array<XJ::XJScriptValue, 1> arguments{XJ::XJScriptVec3{10.0, 20.0, 30.0}};
        auto result = XJ::XJScriptRuntime::Execute(
            instance, FindFunction(*module, "Run"), arguments, host, context);
        Check(result.IsValid() && std::get<double>(*result.ReturnValue) == 3130.0,
              "prefix/postfix writes work in every storage space");
        Check(std::get<XJ::XJScriptVec3>(arguments[0]).Y == 20.0,
              "argument mutations do not alias caller values");
        result = XJ::XJScriptRuntime::Execute(instance, FindFunction(*module, "Math"), {}, host, context);
        Check(result.IsValid() && std::get<XJ::XJScriptVec3>(*result.ReturnValue) == XJ::XJScriptVec3{2.0, 2.0, 2.0},
              "vector arithmetic and ordinary scalar promotion work");

        for (const auto* statement : {"field.x /= 0;", "field *= 1.0e308f;"})
        {
            const auto failing = Compile(
                "private: Vec3 field = Vec3(2.0f, 3.0f, 4.0f); public: void Run() { " +
                std::string(statement) + " }");
            XJ::XJScriptInstance failingInstance;
            Check(!XJ::XJScriptRuntime::InitializeInstance(failingInstance, failing), "fault fixture initializes");
            const auto fault = XJ::XJScriptRuntime::Execute(failingInstance, 0, {}, host, context);
            Check(!fault.IsValid() && failingInstance.IsFaulted() &&
                  std::get<XJ::XJScriptVec3>(*failingInstance.GetField(0)) == XJ::XJScriptVec3{2.0, 3.0, 4.0},
                  "invalid arithmetic faults without partially changing a field");
            Check(!failingInstance.SetField(0, XJ::XJScriptVec3{std::numeric_limits<double>::quiet_NaN(), 0, 0}),
                  "host field setter rejects non-finite vectors");
        }

        auto forged = *module;
        forged.Constants.push_back(XJ::XJScriptEntityRef{7, 0, 1});
        Check(!XJ::XJScriptBytecodeVerifier::Verify(forged).IsValid(), "non-null handle constants are forbidden");
        forged = *module;
        forged.Fields[0].Type = XJ::XJScriptValueType::EntityRef;
        forged.Fields[0].DefaultValue = XJ::XJScriptEntityRef{};
        forged.Fields[0].Access = XJ::XJScriptAccess::Public;
        forged.Fields[0].StableId = 1;
        Check(!XJ::XJScriptBytecodeVerifier::Verify(forged).IsValid(), "forged public handle fields are forbidden");
        forged = *module;
        for (auto& instruction : forged.Functions[0].Code)
            if (instruction.Op == XJ::XJScriptOpCode::Vec3GetComponent) { instruction.A = 3; break; }
        Check(!XJ::XJScriptBytecodeVerifier::Verify(forged).IsValid(), "invalid vector component operands are rejected");

        XJ::XJScriptBytecodeModule uninitialized;
        uninitialized.Constants.push_back(1.0);
        XJ::XJScriptBytecodeFunction function;
        function.Name = "Run";
        function.ReturnType = XJ::XJScriptValueType::Void;
        function.LocalTypes = {XJ::XJScriptValueType::Vec3};
        function.LocalCount = 1;
        function.Code = {
            {XJ::XJScriptOpCode::PushConstant, 0},
            {XJ::XJScriptOpCode::StoreLocalVec3Component, 0, 0},
            {XJ::XJScriptOpCode::ReturnVoid}
        };
        uninitialized.Functions.push_back(std::move(function));
        Check(!XJ::XJScriptBytecodeVerifier::Verify(uninitialized).IsValid(),
              "component store requires an initialized vector");
    }

    class ReturnHost final : public XJ::XJScriptNativeInvoker
    {
    public:
        std::shared_ptr<const XJ::XJScriptNativeCatalogue> Catalogue;
        std::optional<XJ::XJScriptValue> Value;
        unsigned Calls = 0;
        std::shared_ptr<const XJ::XJScriptNativeCatalogue> GetNativeCatalogue() const override { return Catalogue; }
        XJ::XJScriptNativeInvokeResult Invoke(XJ::XJScriptNativeFunctionId,
            std::span<const XJ::XJScriptValue>, XJ::XJScriptNativeContext&) override
        {
            ++Calls;
            return Value ? XJ::XJScriptNativeInvokeResult::Success(*Value)
                         : XJ::XJScriptNativeInvokeResult::Success();
        }
    };

    void TestNativeReturns()
    {
        const auto id = static_cast<XJ::XJScriptNativeFunctionId>(200);
        std::string error;
        auto catalogue = XJ::XJScriptNativeCatalogue::Build({
            {id, "Test", "Read", XJ::XJScriptValueType::Vec3, {}}
        }, error);
        Check(catalogue.has_value(), "custom contract builds");
        auto snapshot = std::make_shared<const XJ::XJScriptNativeCatalogue>(std::move(*catalogue));
        const auto parse = XJ::XJScriptParser::Parse(
            "class Test : ScriptBehaviour { public: Vec3 Run() { return Test.Read(); } }");
        Check(parse.IsValid(), "custom contract source parses");
        const auto semantic = XJ::XJScriptSemanticAnalyzer::Analyze(*parse.Class, snapshot);
        const auto compiled = XJ::XJScriptCompiler::Compile(*parse.Class, semantic);
        Check(compiled.IsValid(), "custom catalogue flows through compiler and verifier");
        auto module = std::make_shared<const XJ::XJScriptBytecodeModule>(*compiled.Module);
        ReturnHost host;
        host.Catalogue = snapshot;
        XJ::XJScriptNativeContext context;
        const std::optional<XJ::XJScriptValue> values[] = {
            XJ::XJScriptVec3{1.0, 2.0, 3.0}, std::string{"wrong"}, std::nullopt,
            XJ::XJScriptVec3{std::numeric_limits<double>::infinity(), 0, 0}
        };
        for (size_t index = 0; index < std::size(values); ++index)
        {
            XJ::XJScriptInstance instance;
            Check(!XJ::XJScriptRuntime::InitializeInstance(instance, module), "return test initializes");
            host.Value = values[index];
            const auto result = XJ::XJScriptRuntime::Execute(instance, 0, {}, host, context);
            Check(result.IsValid() == (index == 0), "native presence, type and finiteness are validated");
        }
        auto different = XJ::XJScriptNativeCatalogue::Build({
            {id, "Test", "Read", XJ::XJScriptValueType::Float, {}}
        }, error);
        host.Catalogue = std::make_shared<const XJ::XJScriptNativeCatalogue>(std::move(*different));
        host.Calls = 0;
        XJ::XJScriptInstance instance;
        Check(!XJ::XJScriptRuntime::InitializeInstance(instance, module), "signature mismatch fixture initializes");
        const auto result = XJ::XJScriptRuntime::Execute(instance, 0, {}, host, context);
        Check(!result.IsValid() && host.Calls == 0, "incompatible host is rejected before native side effects");
    }

    void TestSceneCodec()
    {
        const auto root = std::filesystem::temp_directory_path() / "XJScriptValueIntegrationTests";
        std::filesystem::create_directories(root);
        const auto path = root / "Values.xjscene";
        XJ::XJSceneAsset asset;
        XJ::XJSceneEntityData entity;
        entity.UUID = XJ::XJUUID{1};
        entity.HasScript = true;
        entity.Script.UUID = XJ::XJUUID{2};
        XJ::XJSceneScriptSlotData slot;
        slot.SlotId = XJ::XJUUID{3};
        slot.Script = {4, XJ::XJAssetType::Script};
        slot.FieldOverrides.emplace(5, XJ::XJScriptVec3{1.0, 2.0, 3.0});
        entity.Script.Slots.push_back(std::move(slot));
        asset.Entities.push_back(std::move(entity));
        Check(XJ::XJSceneAssetSerializer::SaveToFile(asset, path), "vector scene saves");
        const auto loaded = XJ::XJSceneAssetSerializer::LoadFromFile(path);
        Check(loaded && loaded->Entities.size() == 1 && loaded->Entities[0].Script.Valid &&
              std::get<XJ::XJScriptVec3>(loaded->Entities[0].Script.Slots[0].FieldOverrides.at(5)) ==
                  XJ::XJScriptVec3{1.0, 2.0, 3.0}, "schema 2 vectors survive full scene round trip");
        asset.Entities[0].Script.Slots[0].FieldOverrides[5] = XJ::XJScriptEntityRef{};
        Check(!XJ::XJSceneAssetSerializer::SaveToFile(asset, path), "runtime references cannot overwrite a scene");
        Check(XJ::XJSceneAssetSerializer::LoadFromFile(path)->Entities[0].Script.Valid,
              "rejected save preserves the prior scene");
        {
            std::ofstream file(path);
            file << R"({"version":3,"objects":[{"uuid":"1","type":"Entity","components":{"script":{"uuid":"2","schemaVersion":999,"slots":[]}}}]})";
            Check(file.good(), "schema fixture written");
        }
        const auto unsupported = XJ::XJSceneAssetSerializer::LoadFromFile(path);
        Check(unsupported && unsupported->Entities.size() == 1 && !unsupported->Entities[0].Script.Valid,
              "empty slots still validate schema version");
        std::filesystem::remove_all(root);
        XJ::XJScriptComponent component;
        const auto slotId = component.AddSlot({4, XJ::XJAssetType::Script})->SlotId;
        Check(!component.SetFieldOverride(slotId, 5, XJ::XJScriptEntityRef{}),
              "component boundary rejects opaque overrides");
    }
}

int main()
{
    const auto asset = XJ::XJScriptAssetCompiler::CompileSource(
        R"(
class ValueContract : ScriptBehaviour
{
private:
    Vec3 stored = Vec3(1.0f, 2.0f, 3.0f);
    EntityRef target = null;

public:
    float Work(Vec3 argument)
    {
        Vec3 value = Vec3(1.0f, 2.0f, 3.0f);
        float old = value.x++;
        value.y += 4;
        ++value.z;
        argument.x += value.x;
        stored.y = value.y;
        return old + argument.x + stored.y;
    }

    bool Empty()
    {
        return target == null;
    }

    Vec3 Make()
    {
        return Vec3(1.0f, 2.0f, 3.0f) * 2.0f;
    }
}
)", "ValueContract.xjs");

    Check(asset && asset->IsCompiled(),
          "new syntax compiles through the complete pipeline");

    XJ::XJScriptInstance instance;
    Check(!XJ::XJScriptRuntime::InitializeInstance(
              instance, asset->Module),
          "typed module initializes");

    Host host;
    XJ::XJScriptNativeContext context;

    const std::array<XJ::XJScriptValue, 1> arguments{
        XJ::XJScriptVec3{10.0, 20.0, 30.0}
    };

    const auto work = XJ::XJScriptRuntime::Execute(
        instance,
        FindFunction(*asset->Module, "Work"),
        arguments,
        host,
        context);

    Check(work.IsValid() && work.ReturnValue &&
          std::get<double>(*work.ReturnValue) == 19.0,
          "field/local/argument component stores preserve value semantics");

    const auto empty = XJ::XJScriptRuntime::Execute(
        instance,
        FindFunction(*asset->Module, "Empty"),
        {},
        host,
        context);

    Check(empty.IsValid() && empty.ReturnValue &&
          std::get<bool>(*empty.ReturnValue),
          "EntityRef null comparison works");

    const auto make = XJ::XJScriptRuntime::Execute(
        instance,
        FindFunction(*asset->Module, "Make"),
        {},
        host,
        context);

    Check(make.IsValid() && make.ReturnValue &&
          std::get<XJ::XJScriptVec3>(*make.ReturnValue) ==
              XJ::XJScriptVec3{2.0, 4.0, 6.0},
          "native Vec3 return is validated and pushed");

    const XJ::XJScriptValue vector = XJ::XJScriptVec3{1.0, 2.0, 3.0};
    const auto encoded = XJ::XJEncodeScriptValue(vector);
    Check(encoded.has_value(), "Vec3 serializes");

    const auto decoded = XJ::XJDecodeScriptValue(*encoded, true);
    Check(decoded && *decoded == vector, "Vec3 round trips");
    Check(!XJ::XJDecodeScriptValue(*encoded, false),
          "old field schema rejects Vec3");
    Check(!XJ::XJEncodeScriptValue(XJ::XJScriptEntityRef{}),
          "runtime EntityRef cannot serialize");

    TestErrorsAndValues();
    TestNativeReturns();
    TestSceneCodec();
    spdlog::info("Script value integration tests passed.");
    return EXIT_SUCCESS;
}
