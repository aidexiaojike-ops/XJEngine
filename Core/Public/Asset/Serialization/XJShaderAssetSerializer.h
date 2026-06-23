#ifndef XJ_SHADER_ASSET_SERIALIZER_H
#define XJ_SHADER_ASSET_SERIALIZER_H

#include "Render/Shader/XJShaderAsset.h"

#include <filesystem>
#include <memory>

namespace XJ
{
    class XJShaderAssetSerializer//读取和存储shader资产
    {
        public:
            static std::shared_ptr<XJShaderAsset> LoadFromFile(const std::filesystem::path& path);//读取shader资产
            static bool SaveToFile(const XJShaderAsset& shaderAsset, const std::filesystem::path& path);//存储shader资产
    };
}

#endif