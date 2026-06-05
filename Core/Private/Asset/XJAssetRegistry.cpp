#include "Asset/XJAssetRegistry.h"
#include <fstream>
#include <nlohmann/json.hpp>   


namespace XJ
{
    bool XJAssetRegistry::RegisterAsset(const XJAssetMeta& meta)
    {
        if(meta.Handle == 0) return false;
        mMetas[meta.Handle] = meta;
        return true;
    }

    bool XJAssetRegistry::RemoveAsset(XJAssetHandle handle)
    {
        return mMetas.erase(handle) > 0;
    }

    bool XJAssetRegistry::Contains(XJAssetHandle handle) const
    {
        return mMetas.count(handle) > 0;
    }

    std::optional<XJAssetMeta> XJAssetRegistry::GetMeta(XJAssetHandle handle) const
    {
        auto it = mMetas.find(handle);
        if(it != mMetas.end()) return it->second;
        return std::nullopt;
    }

    bool XJAssetRegistry::Save(const std::filesystem::path& path) const
    {
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());

        nlohmann::json j;
        for(const auto& [h,m] : mMetas) j[std::to_string(h)] = 
        {
            {"handle", m.Handle},
            {"type", (int)m.Type},
            {"name", m.Name}, 
            {"source", m.SourcePath.string()},
            {"imported", m.ImportedPath.string()}
        };
        std::ofstream out(path);
        out << j.dump(2);
        return out.good();
    }
    bool XJAssetRegistry::Load(const std::filesystem::path& path)
    {
        std::ifstream in(path);
        if(!in) return false;
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(in);
        }
        catch (const nlohmann::json::exception&)
        {
            return false;
        }
        mMetas.clear();
        for(auto& [k,v] : j.items())
        {
            XJAssetMeta m;
            m.Handle         = v["handle"];
            m.Type           = static_cast<XJAssetType>(v["type"].get<int>());
            m.Name           = v["name"].get<std::string>();
            m.SourcePath     = v["source"].get<std::string>();
            m.ImportedPath   = v["imported"].get<std::string>();
            mMetas[m.Handle] = m;
        }
        return true;
    }
    static std::filesystem::path NormalizeAssetPath(const std::filesystem::path& path)//规范化路径，去除冗余的 "." 和 ".." 以及多余的分隔符，确保路径的一致性和可比较性
    {
        return path.lexically_normal().generic_string();
    }

    bool XJAssetRegistry::ContainsSourcePath(const std::filesystem::path& sourcePath) const
    {
        return FindHandleBySourcePath(sourcePath) != 0;
    }

    XJAssetHandle XJAssetRegistry::FindHandleBySourcePath(const std::filesystem::path& sourcePath) const
    {
        const auto normalizedPath = NormalizeAssetPath(sourcePath);
        for(const auto& [handle, meta] : mMetas)
        {
            if(NormalizeAssetPath(meta.SourcePath) == normalizedPath)
                return handle;
        }
        return 0;
    }
}
