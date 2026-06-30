#include "Controllers/XJEditorAssetController.h"

#include "Asset/XJAssetRegistry.h"
#include "Services/XJEditorAssetService.h"
#include "UI/XJEditorUIState.h"

#include <utility>

namespace XJ
{
    void XJEditorAssetController::SetAssetRegistry(XJAssetRegistry* registry)
    {
        mAssetRegistry = registry;
    }

    void XJEditorAssetController::SetRegistryPath(const std::filesystem::path& path)
    {
        mRegistryPath = path;
    }

    void XJEditorAssetController::SetRootPath(const std::filesystem::path& path)
    {
        mRootPath = path;
    }

    void XJEditorAssetController::ProcessRequests(XJEditorUIState& uiState)
    {
        if (!mAssetRegistry)
            return;

        if (uiState.AssetRequests.RequestRefreshRegistry)
        {
            uiState.AssetRequests.RequestRefreshRegistry = false;
            XJEditorAssetService::RefreshRegistry(*mAssetRegistry, mRootPath, mRegistryPath);
        }

        if (uiState.AssetRequests.RequestCreateAsset)
        {
            auto request = uiState.AssetRequests.CreateAsset;
            uiState.AssetRequests.RequestCreateAsset = false;
            uiState.AssetRequests.CreateAsset = {};

            XJAssetHandle createdHandle = 0;
            if (request.Type == XJEditorCreateAssetType::Material)
                createdHandle = XJEditorAssetService::CreateMaterialAsset(*mAssetRegistry, request.Directory, mRegistryPath);
            else if (request.Type == XJEditorCreateAssetType::Scene)
                createdHandle = XJEditorAssetService::CreateSceneAsset(*mAssetRegistry, request.Directory, mRegistryPath);

            if (createdHandle != 0)
            {
                uiState.Selection.SelectedAsset = createdHandle;
                uiState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
                uiState.Selection.HighlightedEntities.clear();
            }
        }

        if (uiState.AssetRequests.RequestRenameAsset)
        {
            auto request = uiState.AssetRequests.RenameAsset;
            uiState.AssetRequests.RequestRenameAsset = false;
            uiState.AssetRequests.RenameAsset = {};

            if (XJEditorAssetService::RenameAsset(*mAssetRegistry, request.Handle, request.Name, mRegistryPath))
            {
                uiState.Selection.SelectedAsset = request.Handle;
                uiState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
                uiState.Selection.HighlightedEntities.clear();
            }
        }

        if (uiState.AssetRequests.RequestDeleteAsset)
        {
            auto request = uiState.AssetRequests.DeleteAsset;
            uiState.AssetRequests.RequestDeleteAsset = false;
            uiState.AssetRequests.DeleteAsset = {};

            if (XJEditorAssetService::DeleteAsset(*mAssetRegistry, request.Handle, mRegistryPath) &&
                uiState.Selection.SelectedAsset == request.Handle)
            {
                uiState.Selection.SelectedAsset = 0;
            }
        }

        if (uiState.AssetRequests.RequestImportExternalFiles)
        {
            auto request = std::move(uiState.AssetRequests.ImportExternalFiles);
            uiState.AssetRequests.RequestImportExternalFiles = false;
            uiState.AssetRequests.ImportExternalFiles = {};

            bool importedAny = false;
            for (const auto& sourcePath : request.SourcePaths)
                importedAny |= XJEditorAssetService::ImportExternalFile(*mAssetRegistry, sourcePath, request.DestinationDirectory);

            if (importedAny)
                mAssetRegistry->Save(mRegistryPath);
        }
    }
}
