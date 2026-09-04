#include "Services/XJEditorAssetService.h"

#include "Asset/Metadata/XJAssetMetadata.h"
#include "Asset/Metadata/XJAssetMetadataSerializer.h"
#include "Asset/Metadata/XJPersistentAssetHandleGenerator.h"
#include "Asset/Metadata/XJAssetMetadataPath.h"

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
#include <optional>                              
#include <utility>
#include <unordered_map> 
#include <vector>

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
                const std::string source = meta.SourcePath.string();
                if (source.empty())
                    continue;

                // builtin:// 等虚拟来源不是文件系统路径，跳过，避免误删内置资产（如 TJCube）。
                if (source.rfind("builtin:", 0) == 0)
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

        bool PathExists(const std::filesystem::path& path)
        {
            std::error_code ec;
            const bool exists = std::filesystem::exists(path, ec);
            return !ec && exists;
        }

        bool IsAssetPathAvailable(const XJAssetRegistry& assetRegistry, const std::filesystem::path& path)
        {
            std::error_code sourceError;
            const bool sourceExists = std::filesystem::exists(path, sourceError);
            std::error_code metadataError;
            const bool metadataExists = std::filesystem::exists(BuildAssetMetadataPath(path), metadataError);
            return !sourceError && !metadataError &&
                   !sourceExists && !metadataExists &&
                   !assetRegistry.ContainsSourcePath(path);
        }

        std::filesystem::path BuildUniqueAssetPath(const XJAssetRegistry& assetRegistry, const std::filesystem::path& directory, const std::string& baseName, const std::string& extension)
        {
            std::filesystem::path targetDirectory = directory.empty() ? std::filesystem::path("Resource") : directory;

            std::string ext = extension;
            if (!ext.empty() && ext.front() != '.')
                ext = "." + ext;

            std::filesystem::path candidate = targetDirectory / (baseName + ext);
            if (IsAssetPathAvailable(assetRegistry, candidate))
                return candidate;

            for (int index = 1; index < 1000; ++index)
            {
                std::filesystem::path numbered = targetDirectory / (baseName + "_" + std::to_string(index) + ext);
                if (IsAssetPathAvailable(assetRegistry, numbered))
                    return numbered;
            }

            return {};
        }

        XJAssetHandle GeneratePersistentAssetHandle(const XJAssetRegistry& assetRegistry)
        {
            const auto generated = XJPersistentAssetHandleGenerator::GenerateUnique(
                [&assetRegistry](XJAssetHandle candidate)
                {
                    return !assetRegistry.Contains(candidate);
                }
            );

            return generated.value_or(XJAsset::InvalidHandle);
        }

        const char* DefaultImporterNameForType(XJAssetType type)
        {
            switch (type)
            {
                case XJAssetType::Mesh:     return "GltfImporter";
                case XJAssetType::Texture:  return "TextureImporter";
                case XJAssetType::Material: return "MaterialSerializer";
                case XJAssetType::Scene:    return "SceneSerializer";
                case XJAssetType::Shader:   return "ShaderSerializer";
                default:                    return "";
            }
        }

        bool RemoveFileForRollback(const std::filesystem::path& path)
        {
            if (path.empty())
                return true;

            std::error_code ec;
            const bool removed = std::filesystem::remove(path, ec);
            if (ec)
            {
                spdlog::critical("Asset transaction rollback failed to remove '{}': {}", path.string(), ec.message());
                return false;
            }

            return removed || !PathExists(path);
        }

        bool RegisterCreatedAsset(XJAssetRegistry& assetRegistry, const std::filesystem::path& path, XJAssetType type, XJAssetHandle handle, const std::filesystem::path& registryPath)
        {
            XJAssetMetadata metadata;
            metadata.Handle = handle;
            metadata.Type = type;
            metadata.Importer = DefaultImporterNameForType(type);
            metadata.ImporterVersion = 1;

            std::string metaError;
            if (!XJAssetMetadataSerializer::Save(path, metadata, &metaError))
            {
                spdlog::error("Failed to write asset metadata '{}': {}", path.string(), metaError);
                RemoveFileForRollback(path);
                return false;
            }


            XJAssetMeta meta;
            meta.Handle = handle;
            meta.Type = type;
            meta.Name = path.stem().string();
            meta.SourcePath = path.lexically_normal().generic_string();
            meta.ImportedPath = "";

            if (!assetRegistry.RegisterAsset(meta))
            {
                RemoveFileForRollback(BuildAssetMetadataPath(path));
                RemoveFileForRollback(path);
                return false;
            }

            if (assetRegistry.Save(registryPath))
                return true;

            // 创建事务失败：撤销内存注册并清理源文件与 sidecar。
            if (!assetRegistry.RemoveAsset(handle))
                spdlog::critical("Asset creation rollback failed to unregister handle {}.", handle);
            RemoveFileForRollback(BuildAssetMetadataPath(path));
            RemoveFileForRollback(path);
            return false;
        }

        std::filesystem::path BuildUniqueImportPath(const XJAssetRegistry& assetRegistry, const std::filesystem::path& desiredPath)
        {
            if (IsAssetPathAvailable(assetRegistry, desiredPath))
                return desiredPath;

            const std::filesystem::path parent = desiredPath.parent_path();
            const std::string stem = desiredPath.stem().string();
            const std::string extension = desiredPath.extension().string();

            for (int index = 1; index < 1000; ++index)
            {
                std::filesystem::path candidate = parent / (stem + "_" + std::to_string(index) + extension);
                if (IsAssetPathAvailable(assetRegistry, candidate))
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

        bool RestoreRenamedAsset(
            XJAssetRegistry& assetRegistry,
            const XJAssetMeta& oldMeta,
            const std::filesystem::path& oldPath,
            const std::filesystem::path& newPath,
            const std::filesystem::path& oldMetadataPath,
            const std::filesystem::path& newMetadataPath,
            bool generatedMetadata)
        {
            // 重命名事务回滚：先恢复文件，再恢复注册表快照。
            bool restored = true;
            if (PathExists(newPath))
                restored = RenameFileNoThrow(newPath, oldPath) && restored;
            if (PathExists(newMetadataPath))
                restored = RenameFileNoThrow(newMetadataPath, oldMetadataPath) && restored;
            if (generatedMetadata && restored && PathExists(oldMetadataPath))
                restored = RemoveFileForRollback(oldMetadataPath) && restored;
            restored = assetRegistry.RegisterAsset(oldMeta) && restored;

            if (!restored)
                spdlog::critical("Asset rename rollback was incomplete for handle {}.", oldMeta.Handle);
            return restored;
        }

        struct TrashedAssetFile
        {
            std::filesystem::path OriginalPath;
            std::filesystem::path TrashPath;
        };

        bool RestoreTrashedFiles(const std::vector<TrashedAssetFile>& files)
        {
            bool restored = true;
            for (auto it = files.rbegin(); it != files.rend(); ++it)
            {
                if (PathExists(it->TrashPath))
                    restored = RenameFileNoThrow(it->TrashPath, it->OriginalPath) && restored;
            }
            return restored;
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

        if (!IsAssetPathAvailable(assetRegistry, newPath))
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

        const std::filesystem::path oldMetadataPath = BuildAssetMetadataPath(oldPath);
        const std::filesystem::path newMetadataPath = BuildAssetMetadataPath(newPath);

        std::error_code ec;
        if (!std::filesystem::is_regular_file(oldPath, ec) || ec)
            return false;

        bool generatedMetadata = false;
        const auto metadataResult = XJAssetMetadataSerializer::Load(oldPath);
        if (metadataResult.Status == XJAssetMetadataLoadStatus::NotFound)
        {
            XJAssetMetadata metadata;
            metadata.Handle = oldMeta.Handle;
            metadata.Type = oldMeta.Type;
            metadata.Importer = DefaultImporterNameForType(oldMeta.Type);
            metadata.ImporterVersion = 1;

            std::string metadataError;
            if (!XJAssetMetadataSerializer::Save(oldPath, metadata, &metadataError))
            {
                spdlog::error("Failed to create metadata before renaming '{}': {}", oldPath.string(), metadataError);
                return false;
            }
            generatedMetadata = true;
        }
        else if (!metadataResult.Succeeded() ||
                 metadataResult.Metadata->Handle != oldMeta.Handle ||
                 metadataResult.Metadata->Type != oldMeta.Type)
        {
            spdlog::error("Asset rename rejected because metadata ownership conflicts with registry: {}", oldPath.string());
            return false;
        }

        // 重命名事务开始：源文件与 sidecar 必须成对移动。
        if (!RenameFileNoThrow(oldPath, newPath))
        {
            if (generatedMetadata)
                RemoveFileForRollback(oldMetadataPath);
            return false;
        }

        if (!RenameFileNoThrow(oldMetadataPath, newMetadataPath))
        {
            const bool sourceRestored = RenameFileNoThrow(newPath, oldPath);
            if (generatedMetadata && sourceRestored)
                RemoveFileForRollback(oldMetadataPath);
            if (!sourceRestored)
                spdlog::critical("Asset rename rollback was incomplete for handle {}.", oldMeta.Handle);
            return false;
        }

        if (!assetRegistry.RegisterAsset(newMeta))
        {
            RestoreRenamedAsset(assetRegistry, oldMeta, oldPath, newPath, oldMetadataPath, newMetadataPath, generatedMetadata);
            return false;
        }

        if (!assetRegistry.Save(registryPath))
        {
            RestoreRenamedAsset(assetRegistry, oldMeta, oldPath, newPath, oldMetadataPath, newMetadataPath, generatedMetadata);
            return false;
        }

        return true;
    }

    XJAssetHandle XJEditorAssetService::CreateMaterialAsset(XJAssetRegistry& assetRegistry, const std::filesystem::path& directory, const std::filesystem::path& registryPath)
    {
        std::filesystem::path path = BuildUniqueAssetPath(assetRegistry, directory, "NewMaterial", ".xjmat");
        if (path.empty())
            return 0;

        XJAssetHandle handle = GeneratePersistentAssetHandle(assetRegistry);
        if (handle == XJAsset::InvalidHandle)
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
        {
            RemoveFileForRollback(path);
            return 0;
        }

        if (!RegisterCreatedAsset(assetRegistry, path, XJAssetType::Material, handle, registryPath))
            return 0;

        return handle;
    }

    XJAssetHandle XJEditorAssetService::CreateSceneAsset(XJAssetRegistry& assetRegistry, const std::filesystem::path& directory, const std::filesystem::path& registryPath)
    {
        std::filesystem::path path = BuildUniqueAssetPath(assetRegistry, directory, "NewScene", ".xjscene");
        if (path.empty())
            return 0;

        XJAssetHandle handle = GeneratePersistentAssetHandle(assetRegistry);
        if (handle == XJAsset::InvalidHandle)
            return 0;

        XJSceneAsset scene;
        scene.mType = XJAssetType::Scene;
        scene.mName = path.stem().string();
        scene.mPath = path;
        scene.Entities.clear();

        if (!XJSceneAssetSerializer::SaveToFile(scene, path))
        {
            RemoveFileForRollback(path);
            return 0;
        }

        if (!RegisterCreatedAsset(assetRegistry, path, XJAssetType::Scene, handle, registryPath))
            return 0;

        return handle;
    }

    bool XJEditorAssetService::DeleteAsset(XJAssetRegistry& assetRegistry, XJAssetHandle handle, const std::filesystem::path& registryPath)
    {
        auto meta = assetRegistry.GetMeta(handle);
        if (!meta)
            return false;

        std::vector<std::filesystem::path> paths;
        auto addPath = [&paths](const std::filesystem::path& path)
        {
            if (path.empty() || path.generic_string().starts_with("builtin:"))
                return;

            const auto normalized = path.lexically_normal();
            const auto duplicate = std::find_if(paths.begin(), paths.end(), [&normalized](const auto& existing)
            {
                return existing.lexically_normal() == normalized;
            });
            if (duplicate == paths.end())
                paths.push_back(normalized);
        };

        addPath(meta->SourcePath);
        addPath(meta->ImportedPath);
        addPath(BuildAssetMetadataPath(meta->SourcePath));

        std::vector<std::filesystem::path> existingPaths;
        for (const auto& path : paths)
        {
            std::error_code ec;
            const bool exists = std::filesystem::exists(path, ec);
            if (ec)
                return false;
            if (!exists)
                continue;
            if (!std::filesystem::is_regular_file(path, ec) || ec)
            {
                spdlog::error("Asset deletion rejected for non-file path: {}", path.string());
                return false;
            }
            existingPaths.push_back(path);
        }

        const std::filesystem::path trashParent = registryPath.parent_path().empty()
            ? std::filesystem::current_path()
            : registryPath.parent_path();
        std::filesystem::path trashPath;
        for (uint32_t attempt = 0; attempt < 128; ++attempt)
        {
            const XJAssetHandle suffix = XJPersistentAssetHandleGenerator::GenerateCandidate();
            const auto candidate = trashParent / (".xjtrash-" + std::to_string(handle) + "-" + std::to_string(suffix));
            if (!PathExists(candidate))
            {
                trashPath = candidate;
                break;
            }
        }
        if (trashPath.empty())
            return false;

        std::error_code ec;
        if (!std::filesystem::create_directories(trashPath, ec) || ec)
            return false;

        // 删除事务开始：先将所有资产文件移入同盘临时回收目录。
        std::vector<TrashedAssetFile> trashedFiles;
        for (size_t index = 0; index < existingPaths.size(); ++index)
        {
            TrashedAssetFile file;
            file.OriginalPath = existingPaths[index];
            file.TrashPath = trashPath / (std::to_string(index) + "-" + existingPaths[index].filename().string());
            if (!RenameFileNoThrow(file.OriginalPath, file.TrashPath))
            {
                RestoreTrashedFiles(trashedFiles);
                std::filesystem::remove_all(trashPath, ec);
                return false;
            }
            trashedFiles.push_back(std::move(file));
        }

        if (!assetRegistry.RemoveAsset(handle) || !assetRegistry.Save(registryPath))
        {
            assetRegistry.RegisterAsset(*meta);
            if (!RestoreTrashedFiles(trashedFiles))
                spdlog::critical("Asset deletion rollback was incomplete for handle {}.", handle);
            ec.clear();
            std::filesystem::remove_all(trashPath, ec);
            return false;
        }

        ec.clear();
        std::filesystem::remove_all(trashPath, ec);
        if (ec)
        {
            // 注册表已提交，保留回收目录供后续人工恢复，避免半清理后伪造可回滚状态。
            spdlog::error("Asset deleted but temporary trash cleanup failed '{}': {}", trashPath.string(), ec.message());
        }
        return true;
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

    bool XJEditorAssetService::ImportExternalFile(XJAssetRegistry& assetRegistry, const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDirectory, const std::filesystem::path& registryPath)
    {
        if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_regular_file(sourcePath))
            return false;

        XJAssetType type = XJAssetRegistryScanner::GetAssetTypeFromExtension(sourcePath);
        if (type == XJAssetType::None)
            return false;

        std::filesystem::path targetDirectory = destinationDirectory.empty() ? std::filesystem::path("Resource") : destinationDirectory;
        std::filesystem::path desiredPath = targetDirectory / sourcePath.filename();
        std::filesystem::path destinationPath = BuildUniqueImportPath(assetRegistry, desiredPath);
        if (destinationPath.empty())
            return false;

        std::error_code ec;
        std::filesystem::create_directories(destinationPath.parent_path(), ec);
        if (ec)
            return false;

        std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::none, ec);
        if (ec)
            return false;

        const XJAssetHandle handle = GeneratePersistentAssetHandle(assetRegistry);
        if (handle == XJAsset::InvalidHandle)
        {
            RemoveFileForRollback(destinationPath);
            return false;
        }

        // 导入事务提交：写入 sidecar、注册内存并原子保存 registry。
        return RegisterCreatedAsset(assetRegistry, destinationPath, type, handle, registryPath);
    }

    bool XJEditorAssetService::RefreshRegistry(XJAssetRegistry& assetRegistry, const std::filesystem::path& rootPath, const std::filesystem::path& registryPath)
    {
        // Detailed scanner 自己构造完整快照并移除缺失项；文件系统错误时不会替换活动 Registry。
        const auto registryBeforeScan = assetRegistry.XJGetAllMetas();
        const XJAssetRegistryScanReport report =
            XJAssetRegistryScanner::ScanResourceAssetsDetailed(assetRegistry, rootPath);

        if (report.FilesystemErrors > 0)
            return false;

        const bool saved = assetRegistry.Save(registryPath);
        if (!saved && !assetRegistry.ReplaceAssets(registryBeforeScan))
            spdlog::critical("Failed to restore registry snapshot after refresh save failure.");
        return saved && report.Succeeded();
    }
}
