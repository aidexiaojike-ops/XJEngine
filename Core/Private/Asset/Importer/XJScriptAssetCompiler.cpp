#include "Asset/Importer/XJScriptAssetCompiler.h"

#include "Script/Compiler/XJScriptCompiler.h"
#include "Script/Language/XJScriptParser.h"
#include "Script/Language/XJScriptSemanticAnalyzer.h"

#include <fstream>
#include <iterator>
#include <string>

namespace XJ
{
    std::shared_ptr<XJScriptAsset>
    XJScriptAssetCompiler::CompileSource(
        std::string_view source,
        const std::filesystem::path&
            virtualPath)
    {
        auto asset =
            std::make_shared<
                XJScriptAsset>();

        asset->mPath = virtualPath;
        asset->mName =
            virtualPath.stem().string();
        asset->Source =
            std::string(source);

        auto parsed =
            XJScriptParser::Parse(source);

        if (!parsed.IsValid())
        {
            asset->Diagnostics =
                std::move(
                    parsed.Diagnostics);

            return asset;
        }

        const auto semantic =
            XJScriptSemanticAnalyzer::
                Analyze(*parsed.Class);

        if (!semantic.IsValid())
        {
            asset->Diagnostics =
                semantic.Diagnostics;

            return asset;
        }

        auto compiled =
            XJScriptCompiler::Compile(
                *parsed.Class,
                semantic);

        if (!compiled.IsValid())
        {
            asset->Diagnostics =
                std::move(
                    compiled.Diagnostics);

            return asset;
        }

        asset->Module =
            std::make_shared<
                const XJScriptBytecodeModule>(
                    std::move(
                        *compiled.Module));

        return asset;
    }

    std::shared_ptr<XJScriptAsset>
    XJScriptAssetCompiler::CompileFile(
        const std::filesystem::path&
            path)
    {
        auto asset =
            std::make_shared<
                XJScriptAsset>();

        asset->mPath = path;
        asset->mName =
            path.stem().string();

        std::ifstream input(
            path,
            std::ios::binary);

        if (!input.is_open())
        {
            asset->Diagnostics.push_back({
                1,
                1,
                "Failed to open script file"
            });
            return asset;
        }

        std::string source{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()
        };
        
        if (input.bad())
        {
            asset->Diagnostics.push_back({
                1,
                1,
                "Failed while reading script file"
            });
        
            return asset;
        }
        
        // 兼容 Windows 编辑器生成的 UTF-8 BOM。
        if (source.size() >= 3 &&
            static_cast<unsigned char>(source[0]) == 0xEF &&
            static_cast<unsigned char>(source[1]) == 0xBB &&
            static_cast<unsigned char>(source[2]) == 0xBF)
        {
            source.erase(0, 3);
        }
        
        return CompileSource(source, path);
    }
}
