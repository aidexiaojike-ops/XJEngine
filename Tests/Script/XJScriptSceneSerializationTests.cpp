#include "Asset/Instantiation/XJSceneInstantiator.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include "Asset/XJAssetRegistry.h"
#include "ECS/Component/XJScriptComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void WriteText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        Check(output.good(), "test scene written");
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "XJScriptSceneSerializationTests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    Check(!ec, "temporary directory created");

    XJ::XJSceneAsset source;
    source.mHandle = 4001;
    source.mName = "ScriptScene";

    XJ::XJSceneEntityData entity;
    entity.UUID = XJ::XJUUID{5001};
    entity.Name = "ScriptEntity";
    entity.HasScript = true;
    entity.Script.UUID = XJ::XJUUID{5002};

    XJ::XJSceneScriptSlotData slot;
    slot.SlotId = XJ::XJUUID{5003};
    slot.Enabled = false;
    slot.Script = {6001, XJ::XJAssetType::Script};
    slot.FieldOverrides.emplace(7001, XJ::XJScriptValue{true});
    slot.FieldOverrides.emplace(7002, XJ::XJScriptValue{int64_t{-9223372036854775807LL}});
    slot.FieldOverrides.emplace(7003, XJ::XJScriptValue{45.5});
    slot.FieldOverrides.emplace(7004, XJ::XJScriptValue{std::string{"hello"}});
    entity.Script.Slots.push_back(std::move(slot));
    source.Entities.push_back(std::move(entity));

    const auto scenePath = root / "ScriptScene.xjscene";
    Check(XJ::XJSceneAssetSerializer::SaveToFile(source, scenePath), "scene saved");
    const auto loaded = XJ::XJSceneAssetSerializer::LoadFromFile(scenePath);
    Check(loaded && loaded->Entities.size() == 1, "scene loaded");
    const auto& loadedScript = loaded->Entities[0].Script;
    Check(loaded->Entities[0].HasScript && loadedScript.Valid, "script component loaded");
    Check(loadedScript.UUID == XJ::XJUUID{5002} && loadedScript.Slots.size() == 1,
          "component identity round trips");
    const auto& loadedSlot = loadedScript.Slots[0];
    Check(loadedSlot.SlotId == XJ::XJUUID{5003} && !loadedSlot.Enabled,
          "slot state round trips");
    Check(loadedSlot.Script.Handle == 6001 && loadedSlot.Script.Type == XJ::XJAssetType::Script,
          "script asset reference round trips");
    Check(std::get<bool>(loadedSlot.FieldOverrides.at(7001)), "bool override round trips");
    Check(std::get<int64_t>(loadedSlot.FieldOverrides.at(7002)) == -9223372036854775807LL,
          "int64 override round trips");
    Check(std::get<double>(loadedSlot.FieldOverrides.at(7003)) == 45.5,
          "float override round trips");
    Check(std::get<std::string>(loadedSlot.FieldOverrides.at(7004)) == "hello",
          "string override round trips");

    XJ::XJScene runtimeScene;
    Check(XJ::XJSceneInstantiator::Instantiate(*loaded, runtimeScene), "script scene instantiates");
    XJ::XJEntity* runtimeEntity = runtimeScene.FindEntityByUUID(XJ::XJUUID{5001});
    Check(runtimeEntity && runtimeEntity->HasComponent<XJ::XJScriptComponent>(),
          "runtime entity receives ScriptComponent");
    const auto rebuilt = XJ::XJSceneAssetSerializer::BuildFromScene(runtimeScene);
    Check(rebuilt && rebuilt->Entities[0].Script.Slots[0].FieldOverrides.size() == 4,
          "runtime scene rebuild preserves overrides");

    const auto scriptPath = root / "Behaviour.xjs";
    WriteText(scriptPath,
        "class Behaviour : ScriptBehaviour { public: "
        "[[FieldId(\"7001\")]] bool flag = false; "
        "[[FieldId(\"7002\")]] int count = 0; "
        "[[FieldId(\"7003\")]] float speed = 1.0f; "
        "[[FieldId(\"7004\")]] string text = \"\"; "
        "void OnUpdate(float dt) { Transform.RotateY(speed * dt); } };" );
    XJ::XJAssetRegistry registry;
    Check(registry.RegisterAsset({6001, XJ::XJAssetType::Script, "Behaviour", scriptPath, {}}),
          "script asset registered for strict validation");
    XJ::XJSceneInstantiateContext strictContext;
    strictContext.Registry = &registry;
    strictContext.RequireCompiledScripts = true;
    XJ::XJScene strictScene;
    Check(XJ::XJSceneInstantiator::Instantiate(*loaded, strictScene, &strictContext),
          "strict Play validation compiles script and accepts matching overrides");

    auto wrongOverride = *loaded;
    wrongOverride.Entities[0].Script.Slots[0].FieldOverrides[7003] = std::string{"wrong"};
    XJ::XJScene rejectedOverrideScene;
    Check(!XJ::XJSceneInstantiator::Instantiate(
              wrongOverride, rejectedOverrideScene, &strictContext),
          "strict Play validation rejects override type mismatch");

    auto nonFiniteSave = source;
    nonFiniteSave.Entities[0].Script.Slots[0].FieldOverrides[7003] =
        std::numeric_limits<double>::infinity();
    Check(!XJ::XJSceneAssetSerializer::SaveToFile(
              nonFiniteSave, root / "NonFinite.xjscene"),
          "non-finite override cannot be saved");

    const auto legacyPath = root / "Legacy.xjscene";
    WriteText(legacyPath,
        R"({"version":2,"asset":{"handle":"1","type":"Scene","name":"Legacy"},"objects":[]})");
    Check(XJ::XJSceneAssetSerializer::LoadFromFile(legacyPath) != nullptr,
          "version 2 scene remains readable");

    const auto overflowVersionPath = root / "OverflowVersion.xjscene";
    WriteText(overflowVersionPath, R"({"version":4294967295,"objects":[]})");
    Check(XJ::XJSceneAssetSerializer::LoadFromFile(overflowVersionPath) == nullptr,
          "out-of-range scene version is rejected without exception");

    const auto invalidPath = root / "Invalid.xjscene";
    WriteText(invalidPath,
        R"({"version":3,"asset":{"handle":"2","type":"Scene","name":"Bad"},"objects":[{"uuid":"8001","type":"Entity","name":"Bad","parent":null,"children":[],"components":{"script":{"uuid":"8002","type":"ScriptComponent","slots":[{"slotId":"0","enabled":true,"script":"asset://6/6001","overrides":[]}]}}}]})");
    const auto invalid = XJ::XJSceneAssetSerializer::LoadFromFile(invalidPath);
    Check(invalid && invalid->Entities[0].HasScript && !invalid->Entities[0].Script.Valid,
          "bad script JSON is marked invalid");
    XJ::XJScene rejectedScene;
    Check(!XJ::XJSceneInstantiator::Instantiate(*invalid, rejectedScene),
          "invalid script component blocks instantiation");

    std::filesystem::remove_all(root, ec);
    std::cout << "XJScript scene serialization tests passed\n";
    return EXIT_SUCCESS;
}
