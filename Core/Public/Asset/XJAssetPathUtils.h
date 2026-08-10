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
