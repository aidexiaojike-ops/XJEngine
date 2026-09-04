#include "Asset/Register/XJAssetRegistryScanner.h"
#include "Asset/Metadata/XJAssetMetadataSerializer.h"
#include "Asset/Metadata/XJPersistentAssetHandleGenerator.h"

#include "Asset/XJAssetPathUtils.h"
#include "Asset/XJAssetRegistry.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <spdlog/spdlog.h>

namespace XJ
{
    namespace
    {
        struct PendingAsset
        {
            std::filesystem::path Path;
            XJAssetType Type = XJAssetType::None;

            XJAssetMetadataLoadResult MetadataResult;

            bool TypeMismatch = false;
            bool DuplicateMetadataHandle = false;
        };

        const char* DefaultImporterName(XJAssetType type)
        {
            switch (type)
            {
                case XJAssetType::Mesh:
                    return "GltfImporter";
                case XJAssetType::Texture:
                    return "TextureImporter";
                case XJAssetType::Material:
                    return "MaterialSerializer";
                case XJAssetType::Scene:
                    return "SceneSerializer";
                case XJAssetType::Shader:
                    return "ShaderSerializer";
                default:
                    return "";
            }
        }
    }
    static std::string ToLower(std::string value)//将字符串转换为小写，便于扩展名比较时忽略大小写
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    XJAssetType XJAssetRegistryScanner::GetAssetTypeFromExtension(const std::filesystem::path& path)
    {
        std::string ext = ToLower(path.extension().string());

        if (ext == ".glb")//读取模型2
            return XJAssetType::Mesh;

        if (ext == ".xjscene")//读取场景
            return XJAssetType::Scene;

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")//读取图片
            return XJAssetType::Texture;

        if (ext == ".xjmat")//读取材质
            return XJAssetType::Material;

        if (ext == ".xjshader")//读取shader
             return XJAssetType::Shader;

        return XJAssetType::None;
    }

    std::string XJAssetRegistryScanner::GetAssetNameFromPath(const std::filesystem::path& path)
    {
        return path.stem().string();
    }

    XJAssetHandle XJAssetRegistryScanner::GenerateStableHandle(const std::filesystem::path& path, XJAssetType type, uint32_t collisionSalt)
    {
        std::string key = NormalizeAssetPathKey(path);//统一使用注册表相同的路径键
        key += "#";
        key += std::to_string(static_cast<int>(type));//加入类型信息，避免不同类型同名文件冲突
        key += "#";
        key += std::to_string(collisionSalt);

        // FNV-1a 64-bit is small, deterministic, and independent of STL implementation.
        // Do not use std::hash for persisted asset IDs: the standard does not require it
        // to stay stable across platforms, STL versions, or engine rebuilds.
        constexpr uint64_t kFnvOffset = 14695981039346656037ull;
        constexpr uint64_t kFnvPrime = 1099511628211ull;

        uint64_t hash = kFnvOffset;
        for (unsigned char c : key)
        {
            hash ^= static_cast<uint64_t>(c);
            hash *= kFnvPrime;
        }

        // Stable registry handles use the low range. Runtime-only handles reserve the high bit.
        hash &= ~XJAsset::RuntimeHandleBit;
         
        if(hash == XJAsset::InvalidHandle) hash = 1;//避免 handle 0，保留给无效句柄
         
        return hash;
    }
    int XJAssetRegistryScanner::ScanResourceAssets(XJAssetRegistry& registry,const std::filesystem::path& resourceRoot)
    {
        const XJAssetRegistryScanReport report = ScanResourceAssetsDetailed(registry, resourceRoot);

        return static_cast<int>(report.Added);
    }
    XJAssetRegistryScanReport XJAssetRegistryScanner::ScanResourceAssetsDetailed(XJAssetRegistry& registry, const std::filesystem::path& resourceRoot)
    {
        XJAssetRegistryScanReport report;
        std::error_code ec;

        auto recordFilesystemError = [&](const std::string& message)
        {
            spdlog::error("{}", message);
            ++report.Errors;
            ++report.FilesystemErrors;
        };

        // 阶段 0：缺失和查询失败分开处理，二者都不能生成一个“完整”快照。
        const bool rootExists = std::filesystem::exists(resourceRoot, ec);
        if (ec)
        {
            recordFilesystemError(
                "Asset scan failed to inspect root '" + resourceRoot.string() + "': " + ec.message());
            return report;
        }

        if (!rootExists)
        {
            spdlog::error("Asset scan root does not exist: '{}'", resourceRoot.string());
            ++report.Errors;
            return report;
        }

        const bool rootIsDirectory = std::filesystem::is_directory(resourceRoot, ec);
        if (ec)
        {
            recordFilesystemError(
                "Asset scan failed to inspect root type '" + resourceRoot.string() + "': " + ec.message());
            return report;
        }

        if (!rootIsDirectory)
        {
            spdlog::error("Asset scan root is not a directory: '{}'", resourceRoot.string());
            ++report.Errors;
            return report;
        }

        std::filesystem::path normalizedRoot = std::filesystem::absolute(resourceRoot, ec);
        if (ec)
        {
            recordFilesystemError(
                "Asset scan failed to resolve root '" + resourceRoot.string() + "': " + ec.message());
            return report;
        }
        normalizedRoot = normalizedRoot.lexically_normal();

        const auto previousMetas = registry.XJGetAllMetas();
        std::unordered_map<std::string, XJAssetHandle> previousHandlesByPath;
        std::unordered_map<XJAssetHandle, XJAssetMeta> nextMetas;

        for (const auto& [handle, meta] : previousMetas)
        {
            previousHandlesByPath.emplace(NormalizeAssetPathKey(meta.SourcePath), handle);
            if (IsBuiltinAssetPath(meta.SourcePath))
                nextMetas.emplace(handle, meta);
        }

        // 阶段 1：只收集支持的源资产，并读取相邻 .xjmeta。
        // 此阶段不修改 Registry，目录枚举不完整时原快照保持不变。
        std::vector<PendingAsset> pendingAssets;
        bool canCommit = true;
        std::filesystem::recursive_directory_iterator iterator(normalizedRoot, ec);
        const std::filesystem::recursive_directory_iterator end;

        if (ec)
        {
            recordFilesystemError("Asset directory iteration failed: " + ec.message());
            canCommit = false;
        }

        while (iterator != end)
        {
            const auto& entry = *iterator;
            std::error_code entryError;
            const bool regularFile = entry.is_regular_file(entryError);

            if (entryError)
            {
                recordFilesystemError(
                    "Asset scan failed to inspect '" + entry.path().string() + "': " + entryError.message());
                canCommit = false;
            }
            else if (regularFile)
            {
                const std::filesystem::path path = entry.path().lexically_normal();
                const XJAssetType type = GetAssetTypeFromExtension(path);

                // .xjmeta 的扩展名不属于资产类型，因此自然跳过。
                if (type != XJAssetType::None)
                {
                    PendingAsset pending;
                    pending.Path = path;
                    pending.Type = type;
                    pending.MetadataResult = XJAssetMetadataSerializer::Load(path);

                    pendingAssets.push_back(std::move(pending));
                }
            }

            std::error_code incrementError;
            iterator.increment(incrementError);
            if (incrementError)
            {
                recordFilesystemError("Asset directory iteration failed: " + incrementError.message());
                canCommit = false;
                break;
            }
        }

        std::sort(
            pendingAssets.begin(),
            pendingAssets.end(),
            [](const PendingAsset& left,
               const PendingAsset& right)
            {
                return NormalizeAssetPathKey(left.Path) < NormalizeAssetPathKey(right.Path);
            });

        // 阶段 2：先全局认领 metadata Handle，复制出来的重复 meta 两边都失效。
        std::unordered_map<XJAssetHandle, size_t> metadataOwner;
        for (size_t index = 0; index < pendingAssets.size(); ++index)
        {
            PendingAsset& pending = pendingAssets[index];
            if (!pending.MetadataResult.Succeeded())
                continue;

            const XJAssetMetadata& metadata = *pending.MetadataResult.Metadata;
            auto [ownerIt, inserted] = metadataOwner.emplace(metadata.Handle, index);
            if (!inserted)
            {
                pendingAssets[ownerIt->second].DuplicateMetadataHandle = true;
                pending.DuplicateMetadataHandle = true;
                spdlog::error(
                    "Duplicate asset metadata handle {}: '{}' and '{}'.",
                    metadata.Handle,
                    pendingAssets[ownerIt->second].Path.string(),
                    pending.Path.string());
            }

            if (metadata.Type != pending.Type)
            {
                pending.TypeMismatch = true;
                spdlog::error(
                    "Asset metadata type mismatch: asset='{}', metaType={}, detectedType={}",
                    pending.Path.string(),
                    static_cast<int>(metadata.Type),
                    static_cast<int>(pending.Type));
            }
        }

        std::unordered_set<XJAssetHandle> claimedHandles;
        for (const auto& [handle, owner] : metadataOwner)
        {
            (void)owner;
            claimedHandles.insert(handle);
        }

        // 阶段 3：从 builtin 和本轮有效 metadata 构造完整候选快照。
        for (PendingAsset& pending : pendingAssets)
        {
            if (pending.TypeMismatch || pending.DuplicateMetadataHandle)
            {
                ++report.Skipped;
                ++report.Errors;
                continue;
            }

            const auto legacyIt = previousHandlesByPath.find(NormalizeAssetPathKey(pending.Path));
            const XJAssetHandle legacyHandle = legacyIt == previousHandlesByPath.end()
                ? XJAsset::InvalidHandle
                : legacyIt->second;
            XJAssetHandle handle = XJAsset::InvalidHandle;

            if (pending.MetadataResult.Succeeded())
            {
                handle = pending.MetadataResult.Metadata->Handle;
                if (legacyHandle != XJAsset::InvalidHandle && legacyHandle != handle)
                {
                    spdlog::error(
                        "Asset identity conflict: path='{}', registryHandle={}, metadataHandle={}.",
                        pending.Path.string(), legacyHandle, handle);
                    ++report.Skipped;
                    ++report.Errors;
                    continue;
                }
            }
            else if (pending.MetadataResult.Status == XJAssetMetadataLoadStatus::NotFound)
            {
                if (legacyHandle != XJAsset::InvalidHandle)
                {
                    if (XJAsset::IsRuntimeHandle(legacyHandle))
                    {
                        spdlog::error("Cannot migrate runtime handle {} for asset '{}'.", legacyHandle, pending.Path.string());
                        ++report.Skipped;
                        ++report.Errors;
                        continue;
                    }
                    // 首次迁移必须沿用旧 Handle，避免场景和材质引用失效。
                    handle = legacyHandle;
                }
                else
                {
                    const auto generated = XJPersistentAssetHandleGenerator::GenerateUnique(
                        [&](XJAssetHandle candidate)
                        {
                            return previousMetas.find(candidate) == previousMetas.end() &&
                                   claimedHandles.find(candidate) == claimedHandles.end();
                        });
                    if (!generated)
                    {
                        spdlog::error("Failed to generate persistent handle for '{}'.", pending.Path.string());
                        ++report.Skipped;
                        ++report.Errors;
                        continue;
                    }
                    handle = *generated;
                }

                XJAssetMetadata metadata;
                metadata.Handle = handle;
                metadata.Type = pending.Type;
                metadata.Importer = DefaultImporterName(pending.Type);
                metadata.ImporterVersion = 1;

                std::string saveError;
                if (!XJAssetMetadataSerializer::Save(pending.Path, metadata, &saveError))
                {
                    recordFilesystemError(
                        "Failed to create metadata for '" + pending.Path.string() + "': " + saveError);
                    canCommit = false;
                    ++report.Skipped;
                    continue;
                }

                ++report.MetadataCreated;
                claimedHandles.insert(handle);
            }
            else
            {
                spdlog::error("Asset metadata is invalid for '{}': {}", pending.Path.string(), pending.MetadataResult.Error);
                ++report.Skipped;
                ++report.Errors;
                if (pending.MetadataResult.Status == XJAssetMetadataLoadStatus::IoError)
                {
                    ++report.FilesystemErrors;
                    canCommit = false;
                }
                continue;
            }

            const auto existingIt = previousMetas.find(handle);
            const XJAssetMeta* existing = existingIt == previousMetas.end() ? nullptr : &existingIt->second;
            if (existing && !AreSameAssetPath(existing->SourcePath, pending.Path))
            {
                if (IsBuiltinAssetPath(existing->SourcePath))
                {
                    spdlog::error("Asset handle {} conflicts with builtin asset '{}'.", handle, existing->SourcePath.string());
                    ++report.Skipped;
                    ++report.Errors;
                    continue;
                }

                std::error_code sourceError;
                const bool oldSourceExists = std::filesystem::exists(existing->SourcePath, sourceError);
                if (sourceError)
                {
                    recordFilesystemError(
                        "Failed to inspect previous asset path '" + existing->SourcePath.string() + "': " + sourceError.message());
                    canCommit = false;
                    ++report.Skipped;
                    continue;
                }

                // 外部移动会保留 .xjmeta；旧源确实不存在时允许 Handle 跟随新路径。
                if (oldSourceExists)
                {
                    spdlog::error(
                        "Asset handle {} already belongs to '{}', cannot register '{}'.",
                        handle, existing->SourcePath.string(), pending.Path.string());
                    ++report.Skipped;
                    ++report.Errors;
                    continue;
                }
            }

            if (nextMetas.find(handle) != nextMetas.end())
            {
                spdlog::error("Asset handle {} cannot be registered for '{}'.", handle, pending.Path.string());
                ++report.Skipped;
                ++report.Errors;
                continue;
            }

            XJAssetMeta registryMeta;
            registryMeta.Handle = handle;
            registryMeta.Type = pending.Type;
            registryMeta.Name = GetAssetNameFromPath(pending.Path);
            registryMeta.SourcePath = pending.Path;
            registryMeta.ImportedPath = existing ? existing->ImportedPath : std::filesystem::path{};
            nextMetas.emplace(handle, std::move(registryMeta));

            if (existing)
                ++report.Updated;
            else
                ++report.Added;
        }

        // 只有文件系统扫描完整时才交换快照；内容坏的 meta 已从完整快照中隔离。
        if (canCommit)
        {
            if (!registry.ReplaceAssets(std::move(nextMetas)))
            {
                spdlog::error("Asset registry rejected the completed scan snapshot.");
                ++report.Errors;
                report.Added = 0;
                report.Updated = 0;
            }
        }
        else
        {
            report.Added = 0;
            report.Updated = 0;
        }

        spdlog::info(
            "Asset scan complete: added={}, updated={}, metadataCreated={}, skipped={}, errors={}, filesystemErrors={}.",
            report.Added,
            report.Updated,
            report.MetadataCreated,
            report.Skipped,
            report.Errors,
            report.FilesystemErrors);

        return report;
    }
}
