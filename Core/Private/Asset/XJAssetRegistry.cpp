#include "Asset/XJAssetRegistry.h"
#include <fstream>
#include <nlohmann/json.hpp>   

#include <spdlog/spdlog.h>

namespace XJ
{
    namespace
    {
        bool IsValidAssetTypeInt(int value)
        {
            return value >= static_cast<int>(XJAssetType::None) &&
                   value <= static_cast<int>(XJAssetType::Shader);
        }
    }

    bool XJAssetRegistry::RegisterAsset(const XJAssetMeta& meta)
    {
        if(meta.Handle == 0) return false;

        std::scoped_lock lock(mMutex);
        mMetas[meta.Handle] = meta;
        return true;
    }

    bool XJAssetRegistry::RemoveAsset(XJAssetHandle handle)
    {
        std::scoped_lock lock(mMutex);
        return mMetas.erase(handle) > 0;
    }

    bool XJAssetRegistry::Contains(XJAssetHandle handle) const
    {
        std::scoped_lock lock(mMutex);
        return mMetas.count(handle) > 0;
    }

    std::optional<XJAssetMeta> XJAssetRegistry::GetMeta(XJAssetHandle handle) const
    {
        std::scoped_lock lock(mMutex);

        auto it = mMetas.find(handle);
        if(it != mMetas.end()) return it->second;
        return std::nullopt;
    }

    std::unordered_map<XJAssetHandle, XJAssetMeta> XJAssetRegistry::XJGetAllMetas() const
    {
        std::scoped_lock lock(mMutex);
        return mMetas;
    }
    //先复制快照，再写文件，避免持锁做 IO：
    bool XJAssetRegistry::Save(const std::filesystem::path& path) const
    {
        const auto metas = XJGetAllMetas();
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());

        nlohmann::json j;
        for(const auto& [h,m] : mMetas) j[std::to_string(h)] = 
        {
            {"handle", m.Handle},
            {"type", static_cast<int>(m.Type)},
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
        catch (const nlohmann::json::exception& e)
        {
            spdlog::error("Asset registry load failed: invalid json '{}': {}", path.string(), e.what());
            return false;
        }

        if (!j.is_object())
        {
            spdlog::error("Asset registry load failed: root must be a json object: {}", path.string());
            return false;
        }
        std::unordered_map<XJAssetHandle, XJAssetMeta> loadedMetas;

        for (const auto& [key, value] : j.items())
        {
               try
            {
                if (!value.is_object())
                {
                    spdlog::warn("Asset registry entry skipped: '{}' is not an object.", key);
                    continue;
                }

                XJAssetMeta meta{};

                // at() 会在字段缺失时抛异常；这里 catch 后跳过坏条目，不让损坏文件直接崩溃。
                meta.Handle = value.at("handle").get<XJAssetHandle>();

                const int typeValue = value.at("type").get<int>();
                if (!IsValidAssetTypeInt(typeValue))
                {
                    spdlog::warn(
                        "Asset registry entry skipped: '{}' has invalid asset type {}.",
                        key,
                        typeValue);
                    continue;
                }

                meta.Type = static_cast<XJAssetType>(typeValue);
                meta.Name = value.at("name").get<std::string>();
                meta.SourcePath = value.at("source").get<std::string>();
                meta.ImportedPath = value.at("imported").get<std::string>();

                if (meta.Handle == 0)
                {
                    spdlog::warn("Asset registry entry skipped: '{}' has zero handle.", key);
                    continue;
                }

                if (meta.Type == XJAssetType::None)
                {
                    spdlog::warn("Asset registry entry skipped: '{}' has asset type None.", key);
                    continue;
                }

                auto [it, inserted] = loadedMetas.emplace(meta.Handle, std::move(meta));
                if (!inserted)
                {
                    spdlog::warn(
                        "Asset registry entry skipped: duplicate handle {} in '{}'.",
                        it->first,
                        path.string());
                    continue;
                }
            }
            catch (const nlohmann::json::exception& e)
            {
                spdlog::warn(
                    "Asset registry entry skipped: '{}' is malformed: {}",
                    key,
                    e.what());
            }
            catch (const std::exception& e)
            {
                spdlog::warn(
                    "Asset registry entry skipped: '{}' failed to load: {}",
                    key,
                    e.what());
            }
        }
        mMetas = std::move(loadedMetas);

        {
            std::scoped_lock lock(mMutex);
            mMetas = std::move(loadedMetas);
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
        const auto metas = XJGetAllMetas();
        const auto normalizedPath = NormalizeAssetPath(sourcePath);
        
        for (const auto& [handle, meta] : metas)
        {
            if (NormalizeAssetPath(meta.SourcePath) == normalizedPath)
                return handle;
        }
    
        return 0;
    }
}
