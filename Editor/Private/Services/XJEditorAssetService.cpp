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

#include <spdlog/spdlog.h>

namespace XJ
{

    namespace
    {
        void RemoveMissingSourceAssets(XJAssetRegistry& assetRegistry)
        {
            const auto metas = assetRegistry.XJGetAllMetas();

            for (const auto& [handle, meta] : metas)
            {
                if (meta.SourcePath.empty())
                    continue;

                std::error_code ec;
                if (std::filesystem::exists(meta.SourcePath, ec))
                    continue;

                ec.clear();
                assetRegistry.RemoveAsset(handle);
                spdlog::warn("Removed stale asset registry entry {} because source path is missing: {}", handle, meta.SourcePath.string());
            }
        }

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
            uint32_t collisionSalt = 0;
            XJAssetHandle handle = XJAssetRegistryScanner::GenerateStableHandle(path, type, collisionSalt);

            while (assetRegistry.Contains(handle))
            {
                ++collisionSalt;
                handle = XJAssetRegistryScanner::GenerateStableHandle(path, type, collisionSalt);
            }

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
        bool RenameFileNoThrow(const std::filesystem::path& from, const std::filesystem::path& to)
        {
            std::error_code ec;
            std::filesystem::rename(from, to, ec);
            if (ec)
            {
                spdlog::error("Failed to rename asset file '{}' -> '{}': {}", from.string(), to.string(), ec.message());
                return false;
            }

            return true;
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

        const XJAssetMeta oldMeta = metaOpt.value();

        const std::string cleanName = TrimAssetName(newName);
        if (cleanName.empty() || ContainsInvalidFileNameCharacter(cleanName))
            return false;

        std::filesystem::path oldPath = oldMeta.SourcePath;
        if (oldPath.empty())
            return false;

        oldPath = oldPath.lexically_normal();

        std::filesystem::path newPath = oldPath.parent_path() / (cleanName + oldPath.extension().string());
        newPath = newPath.lexically_normal();

        if (oldPath.generic_string() == newPath.generic_string())
            return false;

        if (assetRegistry.ContainsSourcePath(newPath) || std::filesystem::exists(newPath))
            return false;

        XJAssetMeta newMeta = oldMeta;
        newMeta.Name = cleanName;
        newMeta.SourcePath = newPath.generic_string();

        // Some engine-created assets use the source file as the imported file.
        // Keep ImportedPath in sync only for that exact case; otherwise imported data stays where it is.
        if (!oldMeta.ImportedPath.empty() &&
            oldMeta.ImportedPath.lexically_normal().generic_string() == oldPath.generic_string())
        {
            newMeta.ImportedPath = newPath.generic_string();
        }

        // From here on, disk and registry must move together. If any later step fails,
        // roll the file and in-memory registry back so RefreshRegistry cannot create a
        // second handle for the renamed file.
        if (!RenameFileNoThrow(oldPath, newPath))
            return false;

        if (!assetRegistry.RegisterAsset(newMeta))
        {
            RenameFileNoThrow(newPath, oldPath);
            assetRegistry.RegisterAsset(oldMeta);
            return false;
        }

        if (!assetRegistry.Save(registryPath))
        {
            RenameFileNoThrow(newPath, oldPath);
            assetRegistry.RegisterAsset(oldMeta);
            return false;
        }

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
        auto meta = assetRegistry.GetMeta(handle);
        if (!meta)
            return false;

        std::error_code ec;

        auto removeFileIfPresent = [&](const std::filesystem::path& path)
        {
            if (path.empty())
                return true;

            if (!std::filesystem::exists(path, ec))
            {
                ec.clear();
                return true;
            }

            if (!std::filesystem::is_regular_file(path, ec))
            {
                ec.clear();
                spdlog::warn("Skip deleting non-file asset path: {}", path.string());
                return true;
            }

            if (!std::filesystem::remove(path, ec))
            {
                spdlog::error("Failed to delete asset file '{}': {}", path.string(), ec.message());
                ec.clear();
                return false;
            }

            return true;
        };

        // Delete files before removing registry entry. If file deletion fails,
        // keep the registry intact so RefreshRegistry cannot silently resurrect state.
        if (!removeFileIfPresent(meta->SourcePath))
            return false;

        if (meta->ImportedPath != meta->SourcePath)
        {
            if (!removeFileIfPresent(meta->ImportedPath))
                return false;
        }

        if (!assetRegistry.RemoveAsset(handle))
            return false;

        return assetRegistry.Save(registryPath);
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
        RemoveMissingSourceAssets(assetRegistry);
        XJAssetRegistryScanner::ScanResourceAssets(assetRegistry, rootPath);
        return assetRegistry.Save(registryPath);
    }
}
