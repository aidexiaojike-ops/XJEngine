#ifndef XJ_ASSET_METADATA_H
#define XJ_ASSET_METADATA_H

#include "Asset/XJAsset.h"

#include <cstdint>
#include <string>

namespace XJ
{
    struct XJAssetMetadata//Metadata 数据结构
    {
        static constexpr uint32_t CurrentVersion = 1;
    
        uint32_t Version = CurrentVersion;
        XJAssetHandle Handle = XJAsset::InvalidHandle;
        XJAssetType Type = XJAssetType::None;

        // 为未来 importer 版本迁移保留。
        std::string Importer;
        uint32_t ImporterVersion = 1;

        bool IsValid() const
        {
            return Version == CurrentVersion &&
                   Handle != XJAsset::InvalidHandle &&
                   !XJAsset::IsRuntimeHandle(Handle) &&
                   Type != XJAssetType::None;
        }
    };


}

#endif
