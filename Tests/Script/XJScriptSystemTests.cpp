#include "ECS/System/XJScriptSystem.h"

#include "Asset/XJAssetRegistry.h"
#include "ECS/Component/XJScriptComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

    XJ::XJAssetRegistry registry;
    Check(registry.RegisterAsset({9001, XJ::XJAssetType::Script, "Lifecycle", scriptPath, {}}),
          "script registered");

    XJ::XJScene scene;
    XJ::XJEntity* entity = scene.CreateEntityWithTransform("ScriptEntity");
    auto& scripts = entity->AddComponent<XJ::XJScriptComponent>();
    auto* slot = scripts.AddSlotWithId(
        XJ::XJUUID{9002}, {9001, XJ::XJAssetType::Script}, true);
    Check(slot != nullptr, "script slot added");
    Check(scripts.SetFieldOverride(XJ::XJUUID{9002}, 1001, XJ::XJScriptValue{20.0}),
          "speed override set");

    XJ::XJScriptSystem system(scene, registry);
    system.OnCreate();
    system.OnUpdate(0.5f);
    system.OnFixedUpdate(1.0f / 60.0f);
    system.OnDestroy();

    const auto& transform = entity->GetComponent<XJ::XJTransformComponent>();
    Check(std::abs(transform.rotation.y - 17.0f) < 0.0001f,
          "all script lifecycle methods execute with override");

    std::filesystem::remove_all(root, ec);
    std::cout << "XJScriptSystem tests passed\n";
    return EXIT_SUCCESS;
}
