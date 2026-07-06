#include "Controllers/XJEditorAssetController.h"

#include "Asset/XJAssetRegistry.h"
#include "Services/XJEditorAssetService.h"
#include "UI/XJEditorUIState.h"

#include <utility>

namespace XJ
{
    void XJEditorAssetController::SetAssetRegistry(XJAssetRegistry* registry)// 设置资产注册表指针
    {
        mAssetRegistry = registry;
    }

    void XJEditorAssetController::SetRegistryPath(const std::filesystem::path& path)// 设置注册表文件的磁盘路径
    {
        mRegistryPath = path;
    }

    void XJEditorAssetController::SetRootPath(const std::filesystem::path& path)// 设置资产根目录（资源文件夹）
    {
        mRootPath = path;
    }

    void XJEditorAssetController::ProcessRequests(XJEditorUIState& uiState)// 处理来自 UI 的资产相关请求（刷新、创建、重命名、删除、导入）
    {
        // 没有注册表则无法进行任何操作
        if (!mAssetRegistry)
            return;
         // ---------- 刷新资产注册表 ----------
        if (uiState.AssetRequests.RequestRefreshRegistry)
        {
            uiState.AssetRequests.RequestRefreshRegistry = false;
            XJEditorAssetService::RefreshRegistry(*mAssetRegistry, mRootPath, mRegistryPath);
        }
        // ---------- 创建资产 ----------
        if (uiState.AssetRequests.RequestCreateAsset)
        {
            // 取出请求并重置
            auto request = uiState.AssetRequests.CreateAsset;
            uiState.AssetRequests.RequestCreateAsset = false;
            uiState.AssetRequests.CreateAsset = {};
            XJAssetHandle createdHandle = 0;
            // 根据请求的资产类型调用不同的创建函数
            if (request.Type == XJEditorCreateAssetType::Material)
                createdHandle = XJEditorAssetService::CreateMaterialAsset(*mAssetRegistry, request.Directory, mRegistryPath);
            else if (request.Type == XJEditorCreateAssetType::Scene)
                createdHandle = XJEditorAssetService::CreateSceneAsset(*mAssetRegistry, request.Directory, mRegistryPath);
            // 创建成功时自动选中新资产，并清除实体选中和高亮
            if (createdHandle != 0)
            {
                uiState.Selection.SelectedAsset = createdHandle;
                uiState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
                uiState.Selection.HighlightedEntities.clear();
            }
        }
        // ---------- 重命名资产 ----------
        if (uiState.AssetRequests.RequestRenameAsset)
        {
            auto request = uiState.AssetRequests.RenameAsset;
            uiState.AssetRequests.RequestRenameAsset = false;
            uiState.AssetRequests.RenameAsset = {};
            // 调用资产服务执行重命名（会修改文件并更新注册表）
            if (XJEditorAssetService::RenameAsset(*mAssetRegistry, request.Handle, request.Name, mRegistryPath))
            {
                uiState.Selection.SelectedAsset = request.Handle;
                uiState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
                uiState.Selection.HighlightedEntities.clear();
            }
        }
        // ---------- 删除资产 ----------
        if (uiState.AssetRequests.RequestDeleteAsset)
        {
            auto request = uiState.AssetRequests.DeleteAsset;
            uiState.AssetRequests.RequestDeleteAsset = false;
            uiState.AssetRequests.DeleteAsset = {};
            // 调用删除服务，如果当前选中的正是被删除的资产，则清空选中
            if (XJEditorAssetService::DeleteAsset(*mAssetRegistry, request.Handle, mRegistryPath) &&
                uiState.Selection.SelectedAsset == request.Handle)
            {
                uiState.Selection.SelectedAsset = 0;
            }
        }
        // ---------- 导入外部文件 ----------
        if (uiState.AssetRequests.RequestImportExternalFiles)
        {
            // 移动请求数据以避免拷贝
            auto request = std::move(uiState.AssetRequests.ImportExternalFiles);
            uiState.AssetRequests.RequestImportExternalFiles = false;
            uiState.AssetRequests.ImportExternalFiles = {};

            bool importedAny = false;
            // 遍历所有源文件路径，逐个导入到目标目录
            for (const auto& sourcePath : request.SourcePaths)
                importedAny |= XJEditorAssetService::ImportExternalFile(*mAssetRegistry, sourcePath, request.DestinationDirectory);
            // 如果至少有一个文件导入成功，立即保存注册表
            if (importedAny)
                mAssetRegistry->Save(mRegistryPath);
        }
    }
}
