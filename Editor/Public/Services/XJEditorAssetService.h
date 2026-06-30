#ifndef XJ_EDITOR_ASSET_SERVICE_H
#define XJ_EDITOR_ASSET_SERVICE_H

#include "Asset/XJAsset.h"

#include <filesystem>
#include <string>
#include <vector>

namespace XJ
{
    class XJAssetRegistry;

    enum class XJEditorAssetValidationSeverity//// 资产验证时的严重性级别
    {
        Info = 0,// 提示信息
        Warning,// 警告
        Error // 错误
    };

    struct XJEditorAssetValidationMessageView// 单条资产验证消息的描述
    {
        XJEditorAssetValidationSeverity Severity = XJEditorAssetValidationSeverity::Info;// 严重性
        std::string ParameterName;
        std::string Message;
    };

    struct XJEditorShaderValidationView  // 着色器验证结果的视图
    {
        bool Valid = false;
        std::vector<XJEditorAssetValidationMessageView> Messages;

        bool Empty() const
        {
            return Messages.empty();
        }

        bool HasErrors() const// 是否包含至少一个错误级别消息
        {
            for (const auto& message : Messages)
            {
                if (message.Severity == XJEditorAssetValidationSeverity::Error)
                    return true;
            }

            return false;
        }

        bool HasWarnings() const// 是否包含至少一个警告级别消息
        {
            for (const auto& message : Messages)
            {
                if (message.Severity == XJEditorAssetValidationSeverity::Warning)
                    return true;
            }

            return false;
        }
    };
    
    struct XJEditorAssetDetailsView// 编辑器资产详细信息视图，汇集资产的基本信息与验证状态
    {
        bool Valid = false; // 视图数据是否有效（资产是否存在等）
        //资产
        XJAssetHandle Handle = 0;
        XJAssetType Type = XJAssetType::None;
        std::string Name;
        std::filesystem::path SourcePath;
        std::filesystem::path ImportedPath;

        bool HasShaderValidation = false;// 是否包含着色器验证信息
        std::filesystem::path ShaderPath; // 对应的着色器文件路径
        XJEditorShaderValidationView ShaderValidation; // 着色器验证详情
    };

    class XJEditorAssetService
    {
        public:
            static XJEditorAssetDetailsView BuildAssetDetailsView(const XJAssetRegistry& assetRegistry, XJAssetHandle handle);// 根据资产注册表与资产句柄，构建一份便于UI展示的资产详情视图
            static bool RenameAsset(XJAssetRegistry& assetRegistry, XJAssetHandle handle, const std::string& newName, const std::filesystem::path& registryPath);// 重命名资产源文件并同步资产注册表
            static XJAssetHandle CreateMaterialAsset(XJAssetRegistry& assetRegistry, const std::filesystem::path& directory, const std::filesystem::path& registryPath);// 创建材质资产并注册
            static XJAssetHandle CreateSceneAsset(XJAssetRegistry& assetRegistry, const std::filesystem::path& directory, const std::filesystem::path& registryPath);// 创建场景资产并注册
            static bool DeleteAsset(XJAssetRegistry& assetRegistry, XJAssetHandle handle, const std::filesystem::path& registryPath);// 从注册表删除资产
            static bool ImportExternalFile(XJAssetRegistry& assetRegistry, const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDirectory);// 导入外部文件并注册
            static bool RefreshRegistry(XJAssetRegistry& assetRegistry, const std::filesystem::path& rootPath, const std::filesystem::path& registryPath);// 重新扫描资源目录
    };
}

#endif
