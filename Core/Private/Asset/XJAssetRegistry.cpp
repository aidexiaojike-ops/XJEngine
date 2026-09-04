#include "Asset/XJAssetRegistry.h"
#include "Asset/XJAssetPathUtils.h"
#include "Asset/Serialization/XJJsonIO.h"
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

        // builtin:// 等虚拟来源不是文件路径，序列化与解析都原样保留。
        bool IsBuiltinSource(const std::string& raw)
        {
            return raw.rfind("builtin:", 0) == 0;
        }

        // 去掉相对路径可能带有的 "Resource/" 前缀，得到相对资源根目录的路径。
        std::filesystem::path StripResourcePrefix(const std::filesystem::path& relative)
        {
            auto it = relative.begin();
            if (it != relative.end() &&
                (*it == "Resource" || *it == L"Resource"))
            {
                std::filesystem::path stripped;
                ++it;
                for (; it != relative.end(); ++it)
                    stripped /= *it;
                return stripped;
            }
            return relative;
        }

        // registry 落盘时，把绝对路径相对资源根目录存储，保证可移植。
        std::string ToRegistryPath(const std::filesystem::path& path, const std::filesystem::path& resourceRoot)
        {
            const std::string raw = path.string();
            if (raw.empty() || IsBuiltinSource(raw))
                return raw;

            // 相对路径（可能是 Resource/... 形式）先绝对化，再相对资源根目录。
            std::error_code ec;
            std::filesystem::path absolute = path.is_absolute()
                ? path
                : std::filesystem::absolute(path, ec);
            if (ec)
                absolute = path;

            const std::filesystem::path relative =
                std::filesystem::relative(absolute, resourceRoot, ec);
            if (!ec && !relative.empty())
                return relative.lexically_normal().generic_string();

            // 无法相对化时（相对路径解析失败/不同盘符），退回原字符串。
            return raw;
        }

        // registry 加载时，把相对路径解析回绝对路径；旧数据中的绝对路径保持原样。
        std::filesystem::path FromRegistryPath(const std::string& raw, const std::filesystem::path& resourceRoot)
        {
            if (raw.empty() || IsBuiltinSource(raw))
                return std::filesystem::path(raw);

            const std::filesystem::path path(raw);
            if (path.is_absolute())
                return path.lexically_normal();

            // 兼容旧数据中可能带有的 "Resource/" 前缀。
            const std::filesystem::path stripped = StripResourcePrefix(path);
            return (resourceRoot / stripped).lexically_normal();
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

    bool XJAssetRegistry::ReplaceAssets(std::unordered_map<XJAssetHandle, XJAssetMeta> metas)
    {
        for (const auto& [handle, meta] : metas)
        {
            if (handle == XJAsset::InvalidHandle || meta.Handle != handle)
                return false;
        }

        std::scoped_lock lock(mMutex);
        mMetas = std::move(metas);
        return true;
    }

    //先复制快照，再写文件，避免持锁做 IO：
    bool XJAssetRegistry::Save(const std::filesystem::path& path) const
    {
        // registry 约定位于 <资源根目录>/Config/ 下，据此反推资源根目录，
        // 把绝对路径转成相对路径落盘，保证 registry 可跨机器/目录复用。
        const std::filesystem::path resourceRoot =
            path.parent_path().parent_path();

        const auto metas = XJGetAllMetas();
        nlohmann::json j;
        for(const auto& [h,m] : metas) j[std::to_string(h)] = 
        {
            {"handle", m.Handle},
            {"type", static_cast<int>(m.Type)},
            {"name", m.Name}, 
            {"source", ToRegistryPath(m.SourcePath, resourceRoot)},
            {"imported", ToRegistryPath(m.ImportedPath, resourceRoot)}
        };
        return WriteJsonFileAtomic(path, j);
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

        // 与 Save 对称：从 registry 路径反推资源根目录，解析相对路径。
        const std::filesystem::path resourceRoot =
            path.parent_path().parent_path();

        std::unordered_map<XJAssetHandle, XJAssetMeta> loadedMetas;
        XJAssetHandle maxRuntimeHandle = 0;

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
                meta.SourcePath = FromRegistryPath(value.at("source").get<std::string>(), resourceRoot);
                meta.ImportedPath = FromRegistryPath(value.at("imported").get<std::string>(), resourceRoot);

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

                if (XJAsset::IsRuntimeHandle(meta.Handle) && meta.Handle > maxRuntimeHandle)
                    maxRuntimeHandle = meta.Handle;

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
        {
            std::scoped_lock lock(mMutex);
            mMetas = std::move(loadedMetas);
        }

        // Registry files should normally contain stable handles only. This keeps the runtime
        // generator safe if older data accidentally persisted temporary runtime handles.
        XJAsset::ReserveGeneratedHandlesUpTo(maxRuntimeHandle);

        return true;
    }
    bool XJAssetRegistry::ContainsSourcePath(const std::filesystem::path& sourcePath) const
    {
        return FindHandleBySourcePath(sourcePath) != 0;
    }

    XJAssetHandle XJAssetRegistry::FindHandleBySourcePath(const std::filesystem::path& sourcePath) const
    {
        const std::string normalizedPath = NormalizeAssetPathKey(sourcePath);
        std::scoped_lock lock(mMutex);
        
        for (const auto& [handle, meta] : mMetas)
        {
            if (NormalizeAssetPathKey(meta.SourcePath) == normalizedPath)
                return handle;
        }
    
        return 0;
    }
}
