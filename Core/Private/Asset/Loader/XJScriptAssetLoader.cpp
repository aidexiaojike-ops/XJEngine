#include "Asset/Loader/XJScriptAssetLoader.h"

#include "Asset/Importer/XJScriptAssetCompiler.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/XJScriptAsset.h"

#include <system_error>

namespace XJ
{
    std::shared_ptr<XJScriptAsset>
    XJScriptAssetLoader::LoadScript(
        XJAssetHandle handle,
        XJScriptAssetLoadContext& context)
    {
        if (handle == 0 ||
            !context.Registry)
        {
            return nullptr;
        }

        const auto meta =
            context.Registry->GetMeta(handle);

        if (!meta ||
            meta->Type !=
                XJAssetType::Script)
        {
            return nullptr;
        }

        std::error_code timeError;
        const auto writeTime =
            std::filesystem::
                last_write_time(
                    meta->SourcePath,
                    timeError);

        if (context.ScriptCache)
        {
            const auto cached =
                context.ScriptCache->find(
                    handle);

            if (cached !=
                    context.ScriptCache->end() &&
                !timeError &&
                cached->second.LastWriteTime ==
                    writeTime)
            {
                return cached->second.Asset;
            }
        }

        auto asset = XJScriptAssetCompiler::CompileFile(meta->SourcePath);

        if (!asset)
            return nullptr;

        asset->mHandle = meta->Handle;
        asset->mName = meta->Name;
        asset->mPath = meta->SourcePath;
        asset->mType = XJAssetType::Script;

        if (context.ScriptCache &&
            !timeError)
        {
            (*context.ScriptCache)[handle] = {
                asset,
                writeTime
            };
        }

        return asset;
    }
}