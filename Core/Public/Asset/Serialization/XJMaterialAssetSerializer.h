#ifndef XJ_MATERIAL_ASSET_SERIALIZER_H
#define XJ_MATERIAL_ASSET_SERIALIZER_H

#include "Asset/XJMaterialAsset.h"

#include <filesystem>
#include <memory>

namespace XJ
{
    class XJMaterialAssetSerializer//读取和存储材质文件
    {
        public:
            static std::shared_ptr<XJMaterialAsset> LoadFromFile(const std::filesystem::path& path);//读取材质文件
            static bool SaveToFile(const XJMaterialAsset& materialAsset, const std::filesystem::path& path);//存储材质文件
    };
}

#endif