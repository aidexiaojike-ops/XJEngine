#ifndef XJ_ASSET_PATH_UTILS_H
#define XJ_ASSET_PATH_UTILS_H

#include <filesystem>

namespace XJ
{
    inline std::filesystem::path ResolveRelativePath(const std::filesystem::path& ownerFile, const std::filesystem::path& referencedPath)
    {
        if (referencedPath.empty())
            return referencedPath;

        if (referencedPath.is_absolute())
            return referencedPath.lexically_normal();

        if (std::filesystem::exists(referencedPath))
            return referencedPath.lexically_normal();

        std::filesystem::path resolved = ownerFile.parent_path() / referencedPath;
        return resolved.lexically_normal();
    }
}

#endif