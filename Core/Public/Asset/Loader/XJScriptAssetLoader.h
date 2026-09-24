#ifndef XJ_SCRIPT_ASSET_LOADER_H
#define XJ_SCRIPT_ASSET_LOADER_H

#include "Asset/XJAsset.h"

#include <filesystem>
#include <memory>
#include <unordered_map>

namespace XJ
{
    class XJAssetRegistry;
    class XJScriptAsset;

    struct XJScriptAssetCacheEntry
    {
        std::shared_ptr<XJScriptAsset>
            Asset;

        std::filesystem::file_time_type
            LastWriteTime{};
    };

    struct XJScriptAssetLoadContext
    {
        XJAssetRegistry* Registry = nullptr;

        std::unordered_map<
            XJAssetHandle,
            XJScriptAssetCacheEntry>*
            ScriptCache = nullptr;
    };

    class XJScriptAssetLoader
    {
         public:
             // 编译失败仍返回 XJScriptAsset，
             // 通过 Diagnostics 向调用方报告错误。
             static std::shared_ptr<XJScriptAsset>
             LoadScript(XJAssetHandle handle, XJScriptAssetLoadContext&context);
    };
}

#endif