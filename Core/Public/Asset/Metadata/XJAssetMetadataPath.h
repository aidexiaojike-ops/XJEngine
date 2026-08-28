#ifndef XJ_ASSET_METADATA_PATH_H
#define XJ_ASSET_METADATA_PATH_H

#include <filesystem>
//Metadata 路径规则
namespace XJ
{

    inline std::filesystem::path BuildAssetMetadataPath(const std::filesystem::path& assetPath)
    {
        if(assetPath.empty())
            return {};

        std::filesystem::path result = assetPath.lexically_normal();

        // 使用追加而不是 replace_extension：
        // Model.glb -> Model.glb.xjmeta
        result += ".xjmeta";

        return result;
    };

    inline bool IsAssetMetadataPath(const std::filesystem::path& path)
    {
        return path.extension() == ".xjmeta";
    }
}

#endif