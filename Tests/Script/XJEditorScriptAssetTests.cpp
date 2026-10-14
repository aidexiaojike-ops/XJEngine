#include "Services/XJEditorAssetService.h"

#include "Asset/XJAssetRegistry.h"
#include "Asset/Importer/XJScriptAssetCompiler.h"
#include "ECS/XJScene.h"
#include "UI/XJEditorUIState.h"
#include "Workspace/XJEditorWorkspace.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include <iterator>
#include <string>
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

    std::string ReadText(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    void TestRuntimeBinding()
    {
        XJ::XJEditorWorkspace workspace;
        auto& state = workspace.GetUIState();
        state.Selection.SelectedEntity = 123;
        state.Selection.SelectedAsset = 456;
        state.Selection.HighlightedEntities.insert(123);
        auto runtime = std::make_unique<XJ::XJScene>();

        workspace.SetMode(XJ::XJEditorWorkspaceMode::PlayReadOnly);
        workspace.SetRuntimeScene(runtime.get());
        Check(state.RuntimeInspection && workspace.GetRuntimeScene() == runtime.get(),
              "runtime inspector binds the runtime scene");
        state.Selection.SelectedEntity = 789;
        state.Selection.SelectedAsset = 0;
        state.Selection.HighlightedEntities.clear();
        state.RuntimeScriptRequests.push_back({});

        // Resume/Pause 不得覆盖进入 Play 前保存的编辑器选择。
        workspace.SetRuntimeScene(runtime.get());
        Check(state.RuntimeScriptRequests.size() == 1,
              "rebinding the same scene preserves pending runtime commands");
        runtime.reset();
        Check(workspace.GetRuntimeScene() == nullptr,
              "expired runtime scene is not exposed as a dangling pointer");
        workspace.SetRuntimeScene(nullptr);
        workspace.SetMode(XJ::XJEditorWorkspaceMode::Edit);
        Check(!state.RuntimeInspection && state.RuntimeScriptRequests.empty(),
              "stop clears runtime inspection and queued commands");
        Check(state.Selection.SelectedEntity == 123 &&
              state.Selection.SelectedAsset == 456 &&
              state.Selection.HighlightedEntities.count(123) == 1,
              "stop restores editor selection and highlights");
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "XJEditorScriptAssetTests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    Check(!ec, "temporary directory created");

    XJ::XJAssetRegistry registry;
    const auto registryPath = root / "AssetRegistry.json";
    const XJ::XJAssetHandle first =
        XJ::XJEditorAssetService::CreateScriptAsset(registry, root, registryPath);
    const XJ::XJAssetHandle second =
        XJ::XJEditorAssetService::CreateScriptAsset(registry, root, registryPath);
    Check(first != 0 && second != 0 && first != second, "unique scripts created");

    const auto firstMeta = registry.GetMeta(first);
    const auto secondMeta = registry.GetMeta(second);
    Check(firstMeta && firstMeta->Type == XJ::XJAssetType::Script,
          "first script registered");
    Check(secondMeta && secondMeta->Type == XJ::XJAssetType::Script,
          "second script registered");
    Check(firstMeta->SourcePath.stem() == "ScriptName", "default script name");
    Check(secondMeta->SourcePath.stem() == "ScriptName_1", "conflicting script name uniquified");
    Check(ReadText(firstMeta->SourcePath) ==
              "class ScriptName : ScriptBehaviour\n{\n}\n",
          "default script source matches file name");
    Check(XJ::XJScriptAssetCompiler::CompileFile(firstMeta->SourcePath)->IsCompiled(),
          "default script source compiles");
    Check(ReadText(secondMeta->SourcePath) ==
              "class ScriptName_1 : ScriptBehaviour\n{\n}\n",
          "unique script class matches file name");

    const std::string edited =
        "class ScriptName : ScriptBehaviour\n{\n"
        "public:\n[[FieldId(\"1001\")]] float speed = 1.0f;\n};\n";
    std::string error;
    Check(XJ::XJEditorAssetService::SaveScriptSource(registry, first, edited, error),
          "script source saved");
    Check(error.empty() && ReadText(firstMeta->SourcePath) == edited,
          "saved source persisted");
    Check(XJ::XJScriptAssetCompiler::CompileFile(firstMeta->SourcePath)->IsCompiled(),
          "saved source compiles");

    TestRuntimeBinding();
    std::filesystem::remove_all(root, ec);
    spdlog::info("XJEditor script asset tests passed.");
    return EXIT_SUCCESS;
}
