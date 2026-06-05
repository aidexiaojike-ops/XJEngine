#include "Asset/Register/XJAssetRegistryScanner.h"

#include "Asset/XJAssetRegistry.h"

#include <string>
#include <algorithm>

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

        if (ext == ".glb")
            return XJAssetType::Mesh;

        if (ext == ".xjscene")
            return XJAssetType::Scene;

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
            return XJAssetType::Texture;

        return XJAssetType::None;
    }

    std::string XJAssetRegistryScanner::GetAssetNameFromPath(const std::filesystem::path& path)
    {
        return path.stem().string();
    }

    XJAssetHandle XJAssetRegistryScanner::GenerateStableHandle(const std::filesystem::path& path, XJAssetType type)
    {
        std::string key = path.lexically_normal().generic_string();//规范化路径，确保同一文件得到相同的 handle
        key += "#";
        key += std::to_string(static_cast<int>(type));//加入类型信息，避免不同类型同名文件冲突

        std::hash<std::string> hasher;
        XJAssetHandle hash = static_cast<XJAssetHandle>(hasher(key));
        
        if(hash == 0) hash = 1;//避免 handle 0，保留给无效句柄
        
        return hash;
    }
    int XJAssetRegistryScanner::ScanResourceAssets(XJAssetRegistry& registry,const std::filesystem::path& resourceRoot)
    {
        int addedCount = 0;

        if(!std::filesystem::exists(resourceRoot))
            return addedCount;

        for(const auto& entry : std::filesystem::recursive_directory_iterator(resourceRoot))
        {
            if(!entry.is_regular_file())
                continue;

            const auto path = entry.path();
            const auto type = GetAssetTypeFromExtension(path);

            if(type == XJAssetType::None)
                continue;   
            if(registry.ContainsSourcePath(path))
                continue;

            XJAssetHandle handle = GenerateStableHandle(path, type);

            while (registry.Contains(handle))
            {
                ++handle;
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