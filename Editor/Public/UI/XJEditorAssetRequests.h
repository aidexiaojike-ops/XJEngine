#ifndef XJ_EDITOR_ASSET_REQUESTS_H
#define XJ_EDITOR_ASSET_REQUESTS_H

#include "Asset/XJAsset.h"

#include <filesystem>
#include <string>
#include <vector>

namespace XJ
{
    enum class XJEditorCreateAssetType
    {
        None = 0,
        Material,
        Scene
    };

    struct XJEditorCreateAssetRequest
    {
        XJEditorCreateAssetType Type = XJEditorCreateAssetType::None;
        std::filesystem::path Directory;
    };

    struct XJEditorRenameAssetRequest
    {
        XJAssetHandle Handle = 0;
        std::string Name;
    };

    struct XJEditorDeleteAssetRequest
    {
        XJAssetHandle Handle = 0;
    };

    struct XJEditorImportExternalFilesRequest
    {
        std::filesystem::path DestinationDirectory;
        std::vector<std::filesystem::path> SourcePaths;
    };

    struct XJEditorAssetRequestState
    {
        bool RequestRefreshRegistry = false;

        bool RequestCreateAsset = false;
        XJEditorCreateAssetRequest CreateAsset;

        bool RequestRenameAsset = false;
        XJEditorRenameAssetRequest RenameAsset;

        bool RequestDeleteAsset = false;
        XJEditorDeleteAssetRequest DeleteAsset;

        bool RequestImportExternalFiles = false;
        XJEditorImportExternalFilesRequest ImportExternalFiles;
    };
}

#endif
