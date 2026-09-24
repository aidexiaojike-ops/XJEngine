#include "Asset/Loader/XJScriptAssetLoader.h"

#include "Asset/XJAssetRegistry.h"
#include "Asset/XJScriptAsset.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <unordered_map>

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

    void Write(const std::filesystem::path& path, const std::string& source)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(source.data(), static_cast<std::streamsize>(source.size()));
        Check(output.good(), "test script written");
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "XJScriptAssetLoaderTests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    Check(!ec, "temporary directory created");

    const auto validPath = root / "Rotate.xjs";
    const auto invalidPath = root / "Invalid.xjs";
    const auto bomPath = root / "Bom.xjs";
    const auto missingPath = root / "Missing.xjs";

    const std::string prefix =
        "class Rotate : ScriptBehaviour { public: "
        "[[FieldId(\"1001\")]] float speed = ";
    const std::string suffix =
        "; void OnUpdate(float dt) { Transform.RotateY(speed * dt); } };";
    Write(validPath, prefix + "1.0f" + suffix);
    Write(invalidPath, "class Invalid : ScriptBehaviour { public: float value = 1.0f; };");
    Write(bomPath, std::string("\xEF\xBB\xBF") + prefix + "2.0f" + suffix);

    XJ::XJAssetRegistry registry;
    Check(registry.RegisterAsset({1001, XJ::XJAssetType::Script, "Rotate", validPath, {}}),
          "valid script registered");
    Check(registry.RegisterAsset({1002, XJ::XJAssetType::Material, "Wrong", validPath, {}}),
          "wrong type registered");
    Check(registry.RegisterAsset({1003, XJ::XJAssetType::Script, "Invalid", invalidPath, {}}),
          "invalid script registered");
    Check(registry.RegisterAsset({1004, XJ::XJAssetType::Script, "Bom", bomPath, {}}),
          "BOM script registered");
    Check(registry.RegisterAsset({1005, XJ::XJAssetType::Script, "Missing", missingPath, {}}),
          "missing script registered");

    std::unordered_map<XJ::XJAssetHandle, XJ::XJScriptAssetCacheEntry> cache;
    XJ::XJScriptAssetLoadContext context{&registry, &cache};

    const auto valid = XJ::XJScriptAssetLoader::LoadScript(1001, context);
    Check(valid && valid->IsCompiled(), "valid handle compiles");
    Check(valid->mHandle == 1001 && valid->mType == XJ::XJAssetType::Script,
          "asset identity restored");
    Check(valid->Module->Fields.size() == 1 &&
              std::get<double>(valid->Module->Fields[0].DefaultValue) == 1.0,
          "compiled field default loaded");

    const auto cached = XJ::XJScriptAssetLoader::LoadScript(1001, context);
    Check(cached == valid, "unchanged file uses cache");

    const auto oldTime = std::filesystem::last_write_time(validPath);
    Write(validPath, prefix + "3.0f" + suffix);
    std::filesystem::last_write_time(validPath, oldTime + std::chrono::seconds(2), ec);
    Check(!ec, "script timestamp advanced");
    const auto recompiled = XJ::XJScriptAssetLoader::LoadScript(1001, context);
    Check(recompiled && recompiled != valid &&
              std::get<double>(recompiled->Module->Fields[0].DefaultValue) == 3.0,
          "changed file recompiles");

    Check(!XJ::XJScriptAssetLoader::LoadScript(0, context), "zero handle rejected");
    Check(!XJ::XJScriptAssetLoader::LoadScript(9999, context), "unknown handle rejected");
    Check(!XJ::XJScriptAssetLoader::LoadScript(1002, context), "wrong asset type rejected");

    const auto invalid = XJ::XJScriptAssetLoader::LoadScript(1003, context);
    Check(invalid && !invalid->IsCompiled() && !invalid->Diagnostics.empty(),
          "compile diagnostics preserved");

    const auto bom = XJ::XJScriptAssetLoader::LoadScript(1004, context);
    Check(bom && bom->IsCompiled(), "UTF-8 BOM accepted");

    const auto missing = XJ::XJScriptAssetLoader::LoadScript(1005, context);
    Check(missing && !missing->IsCompiled() && !missing->Diagnostics.empty(),
          "missing file returns diagnostics");

    std::filesystem::remove_all(root, ec);
    std::cout << "XJScriptAssetLoader tests passed\n";
    return EXIT_SUCCESS;
}
