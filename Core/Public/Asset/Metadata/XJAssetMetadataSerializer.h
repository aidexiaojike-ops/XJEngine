#ifndef XJ_ASSET_METADATA_SERIALIZER_H
#define XJ_ASSET_METADATA_SERIALIZER_H

#include "Asset/Metadata/XJAssetMetadata.h"

#include <filesystem>
#include <optional>
#include <string>

//Serializer 接口
namespace XJ
{
    enum class XJAssetMetadataLoadStatus//加载数据状态
    {
        Success,
        NotFound,//NotFound -> 首次导入，可以创建 meta
        IoError,
        InvalidJson,
        InvalidData,//InvalidData -> meta 损坏，不能静默生成新 Handle
        UnsupportedVersion
    };

    struct XJAssetMetadataLoadResult//加载结果
    {
        XJAssetMetadataLoadStatus Status = XJAssetMetadataLoadStatus::InvalidData;

        std::optional<XJAssetMetadata> Metadata;
        std::string Error;

        bool Succeeded() const
        {
            return Status == XJAssetMetadataLoadStatus::Success && Metadata.has_value();
        }
    };

    class XJAssetMetadataSerializer//序列化
    {
        public:
            // 参数是源资产路径，不是 .xjmeta 路径。
            static XJAssetMetadataLoadResult Load(const std::filesystem::path& assetPath);

            static bool Save(const std::filesystem::path& assetPath,
                    const XJAssetMetadata& metadata,
                    std::string* outError = nullptr);
    };


}

#endif