#ifndef XJ_SCRIPT_ASSET_COMPILER_H
#define XJ_SCRIPT_ASSET_COMPILER_H

#include "Asset/XJScriptAsset.h"

#include <filesystem>
#include <memory>
#include <string_view>

namespace XJ
{
    class XJScriptAssetCompiler
    {
        public:
            static std::shared_ptr<XJScriptAsset>
            CompileSource(
                std::string_view source,
                const std::filesystem::path&
                    virtualPath = {});

            static std::shared_ptr<XJScriptAsset> CompileFile(const std::filesystem::path&path);
    };
}

#endif