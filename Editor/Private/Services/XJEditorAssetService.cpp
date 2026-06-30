#include "Services/XJEditorAssetService.h"

#include "Asset/XJAssetRegistry.h"
#include "Asset/XJMaterialAsset.h"
#include "Asset/XJSceneAsset.h"
#include "Asset/Importer/XJMaterialImporter.h"
#include "Asset/Register/XJAssetRegistryScanner.h"
#include "Asset/Serialization/XJMaterialAssetSerializer.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Render/Shader/XJShaderValidation.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace XJ
{

    namespace
    {
        XJEditorAssetValidationSeverity ToEditorSeverity(XJShaderValidationSeverity severity)//转换为编辑器严重性
        {
            switch (severity)
            {
                case XJShaderValidationSeverity::Warning:
                    return XJEditorAssetValidationSeverity::Warning;

                case XJShaderValidationSeverity::Error:
                    return XJEditorAssetValidationSeverity::Error;

                case XJShaderValidationSeverity::Info:
                default:
                    return XJEditorAssetValidationSeverity::Info;
            }
        }

        XJEditorShaderValidationView ToEditorValidationView(const XJShaderValidationResult& validation)//将后端/内部的数据结构，转换成编辑器 UI 层专用的
        {
            XJEditorShaderValidationView view;
            view.Valid = true;

            for (const auto& message : validation.Messages)
            {
                XJEditorAssetValidationMessageView messageView;
                messageView.Severity = ToEditorSeverity(message.Severity);
                messageView.ParameterName = message.ParameterName;
                messageView.Message = message.Message;

                view.Messages.push_back(std::move(messageView));
            }

            return view;
        }

        void FillShaderValidationFromPath(XJEditorAssetDetailsView& view, const std::filesystem::path& shaderPath)//根据路径填充着色器验证信息
        {
            if (shaderPath.empty())
                return;

            view.HasShaderValidation = true;
            view.ShaderPath = shaderPath;

            auto shaderAsset = XJShaderAssetSerializer::LoadFromFile(shaderPath);
            if (!shaderAsset)
            {
                view.ShaderValidation.Valid = false;

                XJEditorAssetValidationMessageView message;
                message.Severity = XJEditorAssetValidationSeverity::Error;
                message.Message = "Failed to load shader asset: " + shaderPath.generic_string();
                view.ShaderValidation.Messages.push_back(std::move(message));

                return;
            }

            view.ShaderValidation = ToEditorValidationView(shaderAsset->Validation);
        }

        std::string TrimAssetName(const std::string& value)
        {
            const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch)
            {
                return std::isspace(ch) != 0;
            });

            const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch)
            {
                return std::isspace(ch) != 0;
            }).base();

            if (begin >= end)
                return {};

            return std::string(begin, end);
        }

        bool ContainsInvalidFileNameCharacter(const std::string& value)
        {
            return value.find_first_of("<>:\"/\\|?*") != std::string::npos;
        }

        std::filesystem::path BuildUniqueAssetPath(const XJAssetRegistry& assetRegistry, const std::filesystem::path& directory, const std::string& baseName, const std::string& extension)
        {
            std::filesystem::path targetDirectory = directory.empty() ? std::filesystem::path("Resource") : directory;

            std::string ext = extension;
            if (!ext.empty() && ext.front() != '.')
                ext = "." + ext;

            std::filesystem::path candidate = targetDirectory / (baseName + ext);
            if (!std::filesystem::exists(candidate) && !assetRegistry.ContainsSourcePath(candidate))
                return candidate;

            for (int index = 1; index < 1000; ++index)
            {
                std::filesystem::path numbered = targetDirectory / (baseName + "_" + std::to_string(index) + ext);
                if (!std::filesystem::exists(numbered) && !assetRegistry.ContainsSourcePath(numbered))
                    return numbered;
            }

            return {};
        }

        XJAssetHandle BuildUniqueAssetHandle(const XJAssetRegistry& assetRegistry, const std::filesystem::path& path, XJAssetType type)
        {
            XJAssetHandle handle = XJAssetRegistryScanner::GenerateStableHandle(path, type);
            while (assetRegistry.Contains(handle))
                ++handle;

            return handle;
        }

        void RegisterCreatedAsset(XJAssetRegistry& assetRegistry, const std::filesystem::path& path, XJAssetType type, XJAssetHandle handle, const std::filesystem::path& registryPath)
        {
            XJAssetMeta meta;
            meta.Handle = handle;
            meta.Type = type;
            meta.Name = path.stem().string();
            meta.SourcePath = path.lexically_normal().generic_string();
            meta.ImportedPath = "";

            assetRegistry.RegisterAsset(meta);
            assetRegistry.Save(registryPath);
        }

        std::filesystem::path BuildUniqueImportPath(const std::filesystem::path& desiredPath)
        {
            if (!std::filesystem::exists(desiredPath))
                return desiredPath;

            const std::filesystem::path parent = desiredPath.parent_path();
            const std::string stem = desiredPath.stem().string();
            const std::string extension = desiredPath.extension().string();

            for (int index = 1; index < 1000; ++index)
            {
                std::filesystem::path candidate = parent / (stem + "_" + std::to_string(index) + extension);
                if (!std::filesystem::exists(candidate))
                    return candidate;
            }

            return {};
        }

    }

    XJEditorAssetDetailsView XJEditorAssetService::BuildAssetDetailsView(const XJAssetRegistry& assetRegistry, XJAssetHandle handle)
    {
        XJEditorAssetDetailsView view;
        
        auto metaOpt = assetRegistry.GetMeta(handle);
        if(!metaOpt)
            return view;
        
        const auto& meta = metaOpt.value();
        
        view.Valid = true;
        view.Handle = meta.Handle;
        view.Type = meta.Type;
        view.Name = meta.Name;
        view.SourcePath = meta.SourcePath;
        view.ImportedPath = meta.ImportedPath;

        if (meta.Type == XJAssetType::Shader)
        {
            FillShaderValidationFromPath(view, meta.SourcePath);
        }
        else if (meta.Type == XJAssetType::Material)
        {
            auto materialAsset = XJMaterialImporter::ImportMaterial(meta.SourcePath.string());
        
            if (materialAsset && !materialAsset->ShaderPath.empty())
                FillShaderValidationFromPath(view, materialAsset->ShaderPath);
        }

        return view;

    }

    bool XJEditorAssetService::RenameAsset(XJAssetRegistry& assetRegistry, XJAssetHandle handle, const std::string& newName, const std::filesystem::path& registryPath)
    {
        auto metaOpt = assetRegistry.GetMeta(handle);
        if (!metaOpt)
            return false;

        XJAssetMeta meta = metaOpt.value();
        const std::string cleanName = TrimAssetName(newName);
        if (cleanName.empty() || ContainsInvalidFileNameCharacter(cleanName))
            return false;

        std::filesystem::path oldPath = meta.SourcePath;
        if (oldPath.empty())
            return false;

        std::filesystem::path newPath = oldPath.parent_path() / (cleanName + oldPath.extension().string());
        newPath = newPath.lexically_normal();

        if (oldPath.lexically_normal().generic_string() == newPath.generic_string())
            return false;

        if (assetRegistry.ContainsSourcePath(newPath) || std::filesystem::exists(newPath))
            return false;

        std::error_code ec;
        std::filesystem::rename(oldPath, newPath, ec);
        if (ec)
            return false;

        const std::filesystem::path oldImportedPath = meta.ImportedPath;
        meta.Name = cleanName;
        meta.SourcePath = newPath.generic_string();

        if (!oldImportedPath.empty() && oldImportedPath.lexically_normal().generic_string() == oldPath.lexically_normal().generic_string())
            meta.ImportedPath = newPath.generic_string();

        if (!assetRegistry.RegisterAsset(meta))
            return false;

        assetRegistry.Save(registryPath);
        return true;
    }

    XJAssetHandle XJEditorAssetService::CreateMaterialAsset(XJAssetRegistry& assetRegistry, const std::filesystem::path& directory, const std::filesystem::path& registryPath)
    {
        std::filesystem::path path = BuildUniqueAssetPath(assetRegistry, directory, "NewMaterial", ".xjmat");
        if (path.empty())
            return 0;

        XJAssetHandle handle = BuildUniqueAssetHandle(assetRegistry, path, XJAssetType::Material);
        if (handle == 0)
            return 0;

        XJMaterialAsset material;
        material.Version = 2;
        material.mType = XJAssetType::Material;
        material.mName = path.stem().string();
        material.mPath = path;
        material.ShaderPath = "Resource/Shader/Unlit.xjshader";
        material.Parameters.clear();
        material.ParameterOverrides.clear();

        if (!XJMaterialAssetSerializer::SaveToFile(material, path))
            return 0;

        RegisterCreatedAsset(assetRegistry, path, XJAssetType::Material, handle, registryPath);
        return handle;
    }

    XJAssetHandle XJEditorAssetService::CreateSceneAsset(XJAssetRegistry& assetRegistry, const std::filesystem::path& directory, const std::filesystem::path& registryPath)
    {
        std::filesystem::path path = BuildUniqueAssetPath(assetRegistry, directory, "NewScene", ".xjscene");
        if (path.empty())
            return 0;

        XJAssetHandle handle = BuildUniqueAssetHandle(assetRegistry, path, XJAssetType::Scene);
        if (handle == 0)
            return 0;

        XJSceneAsset scene;
        scene.mType = XJAssetType::Scene;
        scene.mName = path.stem().string();
        scene.mPath = path;
        scene.Entities.clear();

        if (!XJSceneAssetSerializer::SaveToFile(scene, path))
            return 0;

        RegisterCreatedAsset(assetRegistry, path, XJAssetType::Scene, handle, registryPath);
        return handle;
    }

    bool XJEditorAssetService::DeleteAsset(XJAssetRegistry& assetRegistry, XJAssetHandle handle, const std::filesystem::path& registryPath)
    {
        if (!assetRegistry.RemoveAsset(handle))
            return false;

        assetRegistry.Save(registryPath);
        return true;
    }

    bool XJEditorAssetService::ImportExternalFile(XJAssetRegistry& assetRegistry, const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDirectory)
    {
        if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_regular_file(sourcePath))
            return false;

        XJAssetType type = XJAssetRegistryScanner::GetAssetTypeFromExtension(sourcePath);
        if (type == XJAssetType::None)
            return false;

        std::filesystem::path targetDirectory = destinationDirectory.empty() ? std::filesystem::path("Resource") : destinationDirectory;
        std::filesystem::path desiredPath = targetDirectory / sourcePath.filename();
        std::filesystem::path destinationPath = BuildUniqueImportPath(desiredPath);
        if (destinationPath.empty())
            return false;

        std::error_code ec;
        std::filesystem::create_directories(destinationPath.parent_path(), ec);
        if (ec)
            return false;

        if (assetRegistry.ContainsSourcePath(destinationPath))
            return false;

        std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::none, ec);
        if (ec)
            return false;

        XJAssetMeta meta;
        meta.Type = type;
        meta.Name = XJAssetRegistryScanner::GetAssetNameFromPath(destinationPath);
        meta.SourcePath = destinationPath.lexically_normal().generic_string();
        meta.ImportedPath = "";
        meta.Handle = BuildUniqueAssetHandle(assetRegistry, destinationPath, type);

        if (meta.Handle == 0)
            return false;

        return assetRegistry.RegisterAsset(meta);
    }

    bool XJEditorAssetService::RefreshRegistry(XJAssetRegistry& assetRegistry, const std::filesystem::path& rootPath, const std::filesystem::path& registryPath)
    {
        XJAssetRegistryScanner::ScanResourceAssets(assetRegistry, rootPath);
        return assetRegistry.Save(registryPath);
    }
}
