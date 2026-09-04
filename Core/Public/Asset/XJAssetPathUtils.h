#ifndef XJ_ASSET_PATH_UTILS_H
#define XJ_ASSET_PATH_UTILS_H

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace XJ
{
    inline bool IsBuiltinAssetPath(const std::filesystem::path& path)
    {
        return path.generic_string().rfind("builtin:", 0) == 0;
    }

    // 文件路径统一转为绝对、词法规范化的键；Windows 文件系统比较忽略大小写。
    inline std::string NormalizeAssetPathKey(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};

        std::filesystem::path normalized = path;
        if (!IsBuiltinAssetPath(path) && path.is_relative())
        {
            std::error_code ec;
            const std::filesystem::path absolute = std::filesystem::absolute(path, ec);
            if (!ec)
                normalized = absolute;
        }

        std::string key = normalized.lexically_normal().generic_string();
#ifdef _WIN32
        std::transform(key.begin(), key.end(), key.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
#endif
        return key;
    }

    inline bool AreSameAssetPath(const std::filesystem::path& left, const std::filesystem::path& right)
    {
        return NormalizeAssetPathKey(left) == NormalizeAssetPathKey(right);
    }

    inline std::filesystem::path ResolveRelativePath(const std::filesystem::path& ownerFile, const std::filesystem::path& referencedPath)
    {
        if (referencedPath.empty())
            return referencedPath;

        if (referencedPath.is_absolute())
            return referencedPath.lexically_normal();

        const std::string generic = referencedPath.generic_string();
        if (generic.rfind("Resource/", 0) == 0 || generic.rfind("Resource\\", 0) == 0)
            return referencedPath.lexically_normal();

        // Asset-local references are resolved relative to the owner file. Do not probe
        // referencedPath against the current working directory; CWD can differ between
        // editor launches and would make the same asset resolve to different files.
        std::filesystem::path resolved = ownerFile.parent_path() / referencedPath;
        return resolved.lexically_normal();
    }
}

#endif
