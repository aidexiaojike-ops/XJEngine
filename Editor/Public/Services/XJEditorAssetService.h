#ifndef XJ_EDITOR_ASSET_SERVICE_H
#define XJ_EDITOR_ASSET_SERVICE_H

#include "Asset/XJAsset.h"
#include "UI/XJEditorAssetViewModel.h"

#include <filesystem>
#include <string>
#include <vector>

namespace XJ
{
    class XJAssetRegistry;

    class XJEditorAssetService
    {
        public:
            static XJEditorAssetDetailsView BuildAssetDetailsView(const XJAssetRegistry& assetRegistry, XJAssetHandle handle);// 根据资产注册表与资产句柄，构建一份便于UI展示的资产详情视图
            static bool RenameAsset(XJAssetRegistry& assetRegistry, XJAssetHandle handle, const std::string& newName, const std::filesystem::path& registryPath);// 重命名资产源文件并同步资产注册表
            static XJAssetHandle CreateMaterialAsset(XJAssetRegistry& assetRegistry, const std::filesystem::path& directory, const std::filesystem::path& registryPath);// 创建材质资产并注册
            static XJAssetHandle CreateSceneAsset(XJAssetRegistry& assetRegistry, const std::filesystem::path& directory, const std::filesystem::path& registryPath);// 创建场景资产并注册
            static bool DeleteAsset(XJAssetRegistry& assetRegistry, XJAssetHandle handle, const std::filesystem::path& registryPath);// 从注册表删除资产
            static bool DeleteEmptyFolder(
                XJAssetRegistry& assetRegistry,
                const std::filesystem::path& folderPath,
                const std::filesystem::path& rootPath,
                const std::filesystem::path& registryPath,
                std::string& outError);// 删除空目录并事务式清理目录下的注册表残留
            static bool ImportExternalFile(XJAssetRegistry& assetRegistry, const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDirectory, const std::filesystem::path& registryPath);// 导入外部文件并注册
            static bool RefreshRegistry(XJAssetRegistry& assetRegistry, const std::filesystem::path& rootPath, const std::filesystem::path& registryPath);// 重新扫描资源目录
    };
}

#endif
