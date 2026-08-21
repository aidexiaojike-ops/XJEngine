#include "Services/XJEditorAssetService.h"

#include "Asset/XJAssetRegistry.h"
#include "Asset/XJMaterialAsset.h"
#include "Asset/XJSceneAsset.h"
#include "Asset/Importer/XJModelImporter.h" 
#include "Asset/Importer/XJMaterialImporter.h"
#include "Asset/Register/XJAssetRegistryScanner.h"
#include "Asset/Serialization/XJMaterialAssetSerializer.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Asset/XJMeshAsset.h"  
#include "Geometry/XJBoundingBox.h" 
#include "Render/Shader/XJShaderValidation.h"

#include <algorithm>
#include <cctype>
#include <utility>
#include <optional>                              
#include <unordered_map> 

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

        XJEditorShaderValidationView LoadShaderValidationView(const std::filesystem::path& shaderPath)
        {
            XJEditorShaderValidationView validation;

            if (shaderPath.empty())
                return validation;

            auto shaderAsset = XJShaderAssetSerializer::LoadFromFile(shaderPath);
            if (!shaderAsset)
            {
                validation.Valid = false;

                XJEditorAssetValidationMessageView message;
                message.Severity = XJEditorAssetValidationSeverity::Error;
                message.Message = "Failed to load shader asset: " + shaderPath.generic_string();
                validation.Messages.push_back(std::move(message));

                return validation;
            }

            validation = ToEditorValidationView(shaderAsset->Validation);
            return validation;
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

        std::string NormalizePathForComparison(const std::filesystem::path& path)
        {
            std::error_code ec;
            std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
            if (ec)
                absolutePath = path;

            std::string value = absolutePath.lexically_normal().generic_string();

#ifdef _WIN32
            // Windows 路径比较不区分大小写，避免 registry 与磁盘大小写不同导致漏清理。
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
#endif

            while (value.size() > 1 && value.back() == '/')
                value.pop_back();

            return value;
        }

        bool IsPathInsideDirectory(const std::filesystem::path& candidate, const std::filesystem::path& directory)
        {
            const std::string candidatePath = NormalizePathForComparison(candidate);
            const std::string directoryPath = NormalizePathForComparison(directory);
            if (candidatePath.empty() || directoryPath.empty())
                return false;

            // 加上分隔符，防止 Resource/Foo2 被误判为 Resource/Foo 的子路径。
            return candidatePath.starts_with(directoryPath + "/");
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

        constexpr size_t kAssetInspectorCacheMaxEntries = 256;

        template <typename Map>
        void TrimCacheIfNeeded(Map& cache)
        {
            if (cache.size() > kAssetInspectorCacheMaxEntries)
                cache.clear();
        }

    }

    std::optional<std::filesystem::file_time_type> GetFileWriteTimeOrNull(const std::filesystem::path& path)
    {
        if (path.empty())
            return std::nullopt;

        std::error_code ec;
        const auto writeTime = std::filesystem::last_write_time(path, ec);
        if (ec)
            return std::nullopt;

        return writeTime;
    }
    
    struct ShaderValidationCacheEntry
    {
        std::filesystem::path ShaderPath;
        std::filesystem::file_time_type WriteTime{};
        XJEditorShaderValidationView Validation;
    };

    struct MaterialValidationCacheEntry
    {
        std::filesystem::path MaterialPath;
        std::filesystem::file_time_type MaterialWriteTime{};
        std::filesystem::path ShaderPath;
        std::filesystem::file_time_type ShaderWriteTime{};
        XJEditorShaderValidationView Validation;
    };

    std::unordered_map<XJAssetHandle, ShaderValidationCacheEntry> gShaderValidationCache;
    std::unordered_map<XJAssetHandle, MaterialValidationCacheEntry> gMaterialValidationCache;

    bool IsShaderValidationCacheValid(const ShaderValidationCacheEntry& entry, const std::filesystem::path& shaderPath)
    {
        if (entry.ShaderPath != shaderPath)
            return false;

        const auto writeTime = GetFileWriteTimeOrNull(shaderPath);
        return writeTime && *writeTime == entry.WriteTime;
    }

    bool IsMaterialValidationCacheValid(const MaterialValidationCacheEntry& entry, const std::filesystem::path& materialPath)
    {
        if (entry.MaterialPath != materialPath)
            return false;

        const auto materialWriteTime = GetFileWriteTimeOrNull(materialPath);
        if (!materialWriteTime || *materialWriteTime != entry.MaterialWriteTime)
            return false;

        const auto shaderWriteTime = GetFileWriteTimeOrNull(entry.ShaderPath);
        return shaderWriteTime && *shaderWriteTime == entry.ShaderWriteTime;
    }

    constexpr const char* kBuiltinTJCubeSource = "builtin://mesh/TJCube";

    bool IsBuiltinTJCubeSource(const std::filesystem::path& sourcePath)
    {
        const std::string source = sourcePath.generic_string();
        return source == kBuiltinTJCubeSource || source == "builtin:/mesh/TJCube";
    }


    struct MeshBoundsCacheEntry
    {
        std::filesystem::path SourcePath;
        std::filesystem::file_time_type WriteTime{};

        XJEditorMeshBoundsView Bounds;
    };

    std::unordered_map<XJAssetHandle, MeshBoundsCacheEntry> gMeshBoundsCache;

    bool IsMeshBoundsCacheValid(const MeshBoundsCacheEntry& entry, const std::filesystem::path& sourcePath)
    {
        if (entry.SourcePath != sourcePath)
            return false;
            
        const auto writeTime = GetFileWriteTimeOrNull(sourcePath);
        return writeTime && *writeTime == entry.WriteTime;
    }

    void FillMeshBoundsFromPath(XJEditorAssetDetailsView& view, const std::filesystem::path& sourcePath)
    {
        if (IsBuiltinTJCubeSource(sourcePath))
        {
            view.HasMeshBounds = true;
            view.MeshBounds.Valid = true;
            view.MeshBounds.Min = glm::vec3(-0.5f);
            view.MeshBounds.Max = glm::vec3(0.5f);
            view.MeshBounds.Center = glm::vec3(0.0f);
            view.MeshBounds.Extents = glm::vec3(0.5f);

            XJEditorMeshBoundsView::SubmeshBounds submesh;
            submesh.SubmeshIndex = 0;
            submesh.MaterialSlot = 0;
            submesh.Min = view.MeshBounds.Min;
            submesh.Max = view.MeshBounds.Max;
            view.MeshBounds.Submeshes.push_back(submesh);

            return;
        }
        XJGltfImporter importer;

        if (!importer.LoadMeshAsset(sourcePath.string()))
            return;

        auto meshAsset = importer.ExtractMesh(0);

        if (!meshAsset)
            return;

        XJBoundingBox total;
        for (const XJMeshPrimitive& primitive : meshAsset->mPrimitives)
        {
            if (!primitive.Bounds.IsValid())
                continue;

            total.Merge(primitive.Bounds);

            XJEditorMeshBoundsView::SubmeshBounds submesh;
            submesh.SubmeshIndex = static_cast<uint32_t>(view.MeshBounds.Submeshes.size());
            submesh.MaterialSlot = primitive.MaterialSlot;
            submesh.Min = primitive.Bounds.Min;
            submesh.Max = primitive.Bounds.Max;

            view.MeshBounds.Submeshes.push_back(submesh);
        }

        if (!total.IsValid())
            return;

        view.HasMeshBounds = true;
        view.MeshBounds.Valid = true;
        view.MeshBounds.Min = total.Min;
        view.MeshBounds.Max = total.Max;
        view.MeshBounds.Center = total.GetCenter();
        view.MeshBounds.Extents = total.GetExtents();
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
            const std::filesystem::path shaderPath = meta.SourcePath.lexically_normal();
            view.HasShaderValidation = true;
            view.ShaderPath = shaderPath;

            auto cacheIt = gShaderValidationCache.find(handle);
            if (cacheIt != gShaderValidationCache.end() &&
                IsShaderValidationCacheValid(cacheIt->second, shaderPath))
            {
                view.ShaderValidation = cacheIt->second.Validation;
            }
            else
            {
                view.ShaderValidation = LoadShaderValidationView(shaderPath);

                ShaderValidationCacheEntry entry;
                entry.ShaderPath = shaderPath;
                entry.Validation = view.ShaderValidation;

                const auto writeTime = GetFileWriteTimeOrNull(shaderPath);
                if (writeTime)
                {
                    entry.WriteTime = *writeTime;
                    gShaderValidationCache[handle] = std::move(entry);
                    TrimCacheIfNeeded(gShaderValidationCache);
                }
            }
        }
        else if (meta.Type == XJAssetType::Material)
        {
            const std::filesystem::path materialPath = meta.SourcePath.lexically_normal();

            auto cacheIt = gMaterialValidationCache.find(handle);
            if (cacheIt != gMaterialValidationCache.end() &&
                IsMaterialValidationCacheValid(cacheIt->second, materialPath))
            {
                view.HasShaderValidation = cacheIt->second.Validation.Valid;
                view.ShaderPath = cacheIt->second.ShaderPath;
                view.ShaderValidation = cacheIt->second.Validation;
            }
            else
            {
                auto materialAsset = XJMaterialImporter::ImportMaterial(materialPath.string());

                if (materialAsset && !materialAsset->ShaderPath.empty())
                {
                    view.HasShaderValidation = true;
                    view.ShaderPath = materialAsset->ShaderPath.lexically_normal();
                    view.ShaderValidation = LoadShaderValidationView(view.ShaderPath);

                    MaterialValidationCacheEntry entry;
                    entry.MaterialPath = materialPath;
                    entry.ShaderPath = view.ShaderPath;
                    entry.Validation = view.ShaderValidation;

                    const auto materialWriteTime = GetFileWriteTimeOrNull(materialPath);
                    const auto shaderWriteTime = GetFileWriteTimeOrNull(view.ShaderPath);
                    if (materialWriteTime && shaderWriteTime)
                    {
                        entry.MaterialWriteTime = *materialWriteTime;
                        entry.ShaderWriteTime = *shaderWriteTime;
                        gMaterialValidationCache[handle] = std::move(entry);
                        TrimCacheIfNeeded(gMaterialValidationCache);
                    }
                }
            }
        }
        else if (meta.Type == XJAssetType::Mesh)
        {
            const std::filesystem::path meshPath = meta.SourcePath.lexically_normal();

            auto cacheIt = gMeshBoundsCache.find(handle);
            if (cacheIt != gMeshBoundsCache.end() &&
                IsMeshBoundsCacheValid(cacheIt->second, meshPath))
            {
                view.HasMeshBounds = cacheIt->second.Bounds.Valid;
                view.MeshBounds = cacheIt->second.Bounds;
            }
            else
            {
                FillMeshBoundsFromPath(view, meshPath);

                // 时间戳读取失败时不缓存（内置 TJCube 无源文件，每次走特判，开销可忽略）。
                MeshBoundsCacheEntry entry;
                entry.SourcePath = meshPath;
                entry.Bounds = view.MeshBounds;

                const auto writeTime = GetFileWriteTimeOrNull(meshPath);
                if (writeTime)
                {
                    entry.WriteTime = *writeTime;
                    gMeshBoundsCache[handle] = std::move(entry);
                    TrimCacheIfNeeded(gMeshBoundsCache);
                }
            }
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

    bool XJEditorAssetService::DeleteEmptyFolder(
        XJAssetRegistry& assetRegistry,
        const std::filesystem::path& folderPath,
        const std::filesystem::path& rootPath,
        const std::filesystem::path& registryPath,
        std::string& outError)
    {
        outError.clear();

        const std::string normalizedFolder = NormalizePathForComparison(folderPath);
        const std::string normalizedRoot = NormalizePathForComparison(rootPath);
        if (normalizedFolder.empty() || normalizedRoot.empty())
        {
            outError = "Invalid folder path.";
            return false;
        }

        if (normalizedFolder == normalizedRoot)
        {
            outError = "The asset root folder cannot be deleted.";
            return false;
        }

        if (!IsPathInsideDirectory(folderPath, rootPath))
        {
            outError = "The folder is outside the asset root.";
            return false;
        }

        std::error_code ec;
        const bool exists = std::filesystem::exists(folderPath, ec);
        if (ec || !exists)
        {
            outError = ec ? "Failed to inspect folder: " + ec.message() : "The folder no longer exists.";
            return false;
        }

        if (!std::filesystem::is_directory(folderPath, ec) || ec)
        {
            outError = ec ? "Failed to inspect folder: " + ec.message() : "The selected path is not a directory.";
            return false;
        }

        if (!std::filesystem::is_empty(folderPath, ec) || ec)
        {
            outError = ec ? "Failed to inspect folder contents: " + ec.message() : "The folder is not empty.";
            return false;
        }

        // 空目录仍可能有陈旧 registry 条目，例如文件被资源管理器直接删除。
        std::vector<XJAssetMeta> staleMetas;
        for (const auto& [handle, meta] : assetRegistry.XJGetAllMetas())
        {
            if (IsPathInsideDirectory(meta.SourcePath, folderPath))
                staleMetas.push_back(meta);
        }

        const bool removed = std::filesystem::remove(folderPath, ec);
        if (ec || !removed)
        {
            outError = ec ? "Failed to delete folder: " + ec.message() : "The folder could not be deleted.";
            return false;
        }

        for (const XJAssetMeta& meta : staleMetas)
            assetRegistry.RemoveAsset(meta.Handle);

        if (assetRegistry.Save(registryPath))
            return true;

        // registry 落盘失败时恢复内存条目与空目录，尽量保持三方状态一致。
        for (const XJAssetMeta& meta : staleMetas)
            assetRegistry.RegisterAsset(meta);

        ec.clear();
        std::filesystem::create_directory(folderPath, ec);
        if (ec)
        {
            spdlog::critical(
                "Delete folder rollback failed for '{}': {}",
                folderPath.string(),
                ec.message());
        }

        outError = "Failed to save the asset registry. The deletion was rolled back.";
        return false;
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
