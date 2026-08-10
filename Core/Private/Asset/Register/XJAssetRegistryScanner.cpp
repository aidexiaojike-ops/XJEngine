#include "Asset/Register/XJAssetRegistryScanner.h"

#include "Asset/XJAssetRegistry.h"

#include <string>
#include <algorithm>
#include <vector>

namespace XJ
{
    static std::string ToLower(std::string value)//将字符串转换为小写，便于扩展名比较时忽略大小写
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    XJAssetType XJAssetRegistryScanner::GetAssetTypeFromExtension(const std::filesystem::path& path)
    {
        std::string ext = ToLower(path.extension().string());

        if (ext == ".glb")//读取模型2
            return XJAssetType::Mesh;

        if (ext == ".xjscene")//读取场景
            return XJAssetType::Scene;

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")//读取图片
            return XJAssetType::Texture;

        if (ext == ".xjmat")//读取材质
            return XJAssetType::Material;

        if (ext == ".xjshader")//读取shader
             return XJAssetType::Shader;

        return XJAssetType::None;
    }

    std::string XJAssetRegistryScanner::GetAssetNameFromPath(const std::filesystem::path& path)
    {
        return path.stem().string();
    }

    XJAssetHandle XJAssetRegistryScanner::GenerateStableHandle(const std::filesystem::path& path, XJAssetType type, uint32_t collisionSalt)
    {
        std::string key = path.lexically_normal().generic_string();//规范化路径，确保同一文件得到相同的 handle
        key += "#";
        key += std::to_string(static_cast<int>(type));//加入类型信息，避免不同类型同名文件冲突
        key += "#";
        key += std::to_string(collisionSalt);

        // FNV-1a 64-bit is small, deterministic, and independent of STL implementation.
        // Do not use std::hash for persisted asset IDs: the standard does not require it
        // to stay stable across platforms, STL versions, or engine rebuilds.
        constexpr uint64_t kFnvOffset = 14695981039346656037ull;
        constexpr uint64_t kFnvPrime = 1099511628211ull;

        uint64_t hash = kFnvOffset;
        for (unsigned char c : key)
        {
            hash ^= static_cast<uint64_t>(c);
            hash *= kFnvPrime;
        }

        // Stable registry handles use the low range. Runtime-only handles reserve the high bit.
        hash &= ~XJAsset::RuntimeHandleBit;
         
        if(hash == XJAsset::InvalidHandle) hash = 1;//避免 handle 0，保留给无效句柄
         
        return hash;
    }
    int XJAssetRegistryScanner::ScanResourceAssets(XJAssetRegistry& registry,const std::filesystem::path& resourceRoot)
    {
        int addedCount = 0;

        if(!std::filesystem::exists(resourceRoot))
            return addedCount;

        std::vector<std::filesystem::path> assetPaths;

        for(const auto& entry : std::filesystem::recursive_directory_iterator(resourceRoot))
        {
            if(!entry.is_regular_file())
                continue;

            const auto path = entry.path();
            const auto type = GetAssetTypeFromExtension(path);

            if(type == XJAssetType::None)
                continue;   

            assetPaths.push_back(path.lexically_normal());
        }

        std::sort(assetPaths.begin(), assetPaths.end(),
            [](const std::filesystem::path& a, const std::filesystem::path& b)
            {
                return a.generic_string() < b.generic_string();
            });

        for(const auto& path : assetPaths)
        {
            const auto type = GetAssetTypeFromExtension(path);

            if(registry.ContainsSourcePath(path))
                continue;

            uint32_t collisionSalt = 0;
            XJAssetHandle handle = GenerateStableHandle(path, type, collisionSalt);

            while (registry.Contains(handle))
            {
                ++collisionSalt;
                handle = GenerateStableHandle(path, type, collisionSalt);
            }

            XJAssetMeta meta;
            meta.Handle = handle;
            meta.Type = type;
            meta.Name = GetAssetNameFromPath(path);
            meta.SourcePath = path.lexically_normal().generic_string();
            meta.ImportedPath = "";//导入路径可以在后续的导入过程中设置

            if(registry.RegisterAsset(meta))
                ++addedCount;
            
        }

        return addedCount;
    }
}
