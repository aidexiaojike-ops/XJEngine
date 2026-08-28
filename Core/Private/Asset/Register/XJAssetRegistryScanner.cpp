#include "Asset/Register/XJAssetRegistryScanner.h"
#include "Asset/Metadata/XJAssetMetadataSerializer.h"
#include "Asset/Metadata/XJPersistentAssetHandleGenerator.h"

#include "Asset/XJAssetRegistry.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <algorithm>
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

        std::string NormalizePath(const std::filesystem::path& path)
        {
            std::string result =
                path.lexically_normal().generic_string();

#ifdef _WIN32
            std::transform(
                result.begin(),
                result.end(),
                result.begin(),
                [](unsigned char ch)
                {
                    return static_cast<char>(
                        std::tolower(ch));
                });
#endif

            return result;
        }

        bool SamePath(const std::filesystem::path& left,const std::filesystem::path& right)
        {
            return NormalizePath(left) ==
                   NormalizePath(right);
        }

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
        std::string key = path.lexically_normal().generic_string();//规范化路径，确保同一文件得到相同的 handle
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

        // 阶段 0：只验证扫描根目录。本函数不负责创建 Resource 根目录。
        if (!std::filesystem::exists(resourceRoot, ec) || ec)
        {
            if (ec)
            {
                spdlog::error("Asset scan failed for '{}': {}", resourceRoot.string(), ec.message());
                ++report.Errors;
            }

            return report;
        }

        if (!std::filesystem::is_directory(resourceRoot, ec) || ec)
        {
            spdlog::error("Asset scan root is not a directory: '{}'", resourceRoot.string());
            ++report.Errors;
            return report;
        }

        // 阶段 1：只收集支持的源资产，并读取相邻 .xjmeta。
        // 此阶段不修改 Registry，保证后续可以先做全局重复 Handle 检查。
        std::vector<PendingAsset> pendingAssets;

        std::filesystem::recursive_directory_iterator iterator(resourceRoot, std::filesystem::directory_options:: skip_permission_denied, ec);

        const std::filesystem::recursive_directory_iterator end;

        while (!ec && iterator != end)
        {
            const auto& entry = *iterator;

            std::error_code entryError;
            const bool regularFile = entry.is_regular_file(entryError);

            if (!entryError && regularFile)
            {
                const std::filesystem::path path =
                    entry.path().lexically_normal();

                const XJAssetType type =
                    GetAssetTypeFromExtension(path);

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

            iterator.increment(ec);
        }

        if (ec)
        {
            spdlog::error(
                "Asset directory iteration failed: {}",
                ec.message());

            ++report.Errors;
        }

        std::sort(
            pendingAssets.begin(),
            pendingAssets.end(),
            [](const PendingAsset& left,
               const PendingAsset& right)
            {
                return left.Path.generic_string() <
                       right.Path.generic_string();
            });
        
            // 阶段 2：预检所有有效 metadata。
            // metadataOwner 记录“永久 Handle 首次由哪个文件声明”，用于检测两个
            // .xjmeta 复制粘贴后携带同一 Handle 的情况。
            std::unordered_map<XJAssetHandle, size_t> metadataOwner;

            for (size_t index = 0; index < pendingAssets.size(); ++index)
            {
                PendingAsset& pending = pendingAssets[index];
            
                if (!pending.MetadataResult.Succeeded())
                    continue;
            
                const XJAssetMetadata& metadata = *pending.MetadataResult.Metadata;

                // 即使 metadata 类型错误，也先认领其 Handle。这样新资产生成随机
                // Handle 时不会撞到磁盘上一个暂时损坏、等待用户修复的 .xjmeta。
                auto [ownerIt, inserted] = metadataOwner.emplace(metadata.Handle, index);
                    
                if (!inserted)
                {
                    pendingAssets[ownerIt->second].DuplicateMetadataHandle = true;
                    
                    pending.DuplicateMetadataHandle = true;
                    
                    spdlog::error(
                        "Duplicate asset metadata handle {}: "
                        "'{}' and '{}'.",
                        metadata.Handle,
                        pendingAssets[ownerIt->second]
                            .Path.string(),
                        pending.Path.string());

                    continue;
                }

                // 源文件扩展名推导出的类型必须和 .xjmeta 声明一致。
                if (metadata.Type != pending.Type)
                {
                    pending.TypeMismatch = true;

                    spdlog::error(
                        "Asset metadata type mismatch: "
                        "asset='{}', metaType={}, detectedType={}",
                        pending.Path.string(),
                        static_cast<int>(metadata.Type),
                        static_cast<int>(pending.Type));
                }
            }
        
            // claimedHandles 包含磁盘上所有可读取的 meta Handle，以及本轮随后
            // 创建的新 Handle。随机生成器必须同时避开它和 Registry。
            std::unordered_set<XJAssetHandle> claimedHandles;
        
            for (const auto& [handle, owner] : metadataOwner)
            {
                (void)owner;
                claimedHandles.insert(handle);
            }

            // 阶段 3：逐资产解析最终身份。
            // 优先级：有效 .xjmeta > 同路径旧 Registry Handle > 新随机 Handle。
            for (PendingAsset& pending : pendingAssets)
            {
                if (pending.TypeMismatch || pending.DuplicateMetadataHandle)
                {
                    ++report.Skipped;
                    ++report.Errors;
                    continue;
                }
            
                const XJAssetHandle legacyHandle = registry.FindHandleBySourcePath(pending.Path);
                    
                XJAssetHandle handle = XJAsset::InvalidHandle;
                    
                if (pending.MetadataResult.Succeeded())
                {
                    const XJAssetMetadata& metadata = *pending.MetadataResult.Metadata;
                
                    handle = metadata.Handle;
                
                    // 同一路径的旧 Registry Handle 与 meta 不一致时，
                    // 不能擅自选择一边，否则会破坏 Scene 引用。
                    if (legacyHandle != XJAsset::InvalidHandle && legacyHandle != handle)
                    {
                        spdlog::error(
                            "Asset identity conflict: "
                            "path='{}', registryHandle={}, "
                            "metadataHandle={}.",
                            pending.Path.string(),
                            legacyHandle,
                            handle);
                        
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
                            spdlog::error(
                                "Cannot migrate runtime handle {} "
                                "for asset '{}'.",
                                legacyHandle,
                                pending.Path.string());
                            
                            ++report.Skipped;
                            ++report.Errors;
                            continue;
                        }
                    
                        // 核心兼容规则：第一次启用 .xjmeta 时复用旧 Registry Handle，
                        // 否则已有 .xjscene/.xjmat 中的 Handle 引用会全部失效。
                        handle = legacyHandle;
                    }
                    else
                    {
                        const auto generated = XJPersistentAssetHandleGenerator::
                                GenerateUnique(
                                    [&](XJAssetHandle candidate)
                                    {
                                        return
                                            !registry.Contains(
                                                candidate) &&
                                            claimedHandles.find(
                                                candidate) ==
                                                claimedHandles.end();
                                    });
                                
                        if (!generated)
                        {
                            spdlog::error(
                                "Failed to generate persistent "
                                "handle for '{}'.",
                                pending.Path.string());
                            
                            ++report.Skipped;
                            ++report.Errors;
                            continue;
                        }
                    
                        handle = *generated;
                    }
                
                    // 只有 NotFound 才允许创建 metadata。损坏、版本不支持等状态
                    // 绝不能静默覆盖，否则资产会在用户不知情时获得新身份。
                    XJAssetMetadata metadata;
                    metadata.Handle = handle;
                    metadata.Type = pending.Type;
                    metadata.Importer = DefaultImporterName(pending.Type);
                    metadata.ImporterVersion = 1;
                
                    std::string saveError;
                
                    if (!XJAssetMetadataSerializer::Save(
                            pending.Path,
                            metadata,
                            &saveError))
                    {
                        spdlog::error(
                            "Failed to create metadata for '{}': {}",
                            pending.Path.string(),
                            saveError);
                        
                        ++report.Skipped;
                        ++report.Errors;
                        continue;
                    }
                
                    ++report.MetadataCreated;
                    claimedHandles.insert(handle);
                }
                else
                {
                    // InvalidJson/InvalidData/UnsupportedVersion：
                    // 禁止自动生成新 Handle。
                    spdlog::error(
                        "Asset metadata is invalid for '{}': {}",
                        pending.Path.string(),
                        pending.MetadataResult.Error);
                    
                    ++report.Skipped;
                    ++report.Errors;
                    continue;
                }

                // 阶段 4：写入 Registry 前做最后一次“Handle -> Path”冲突检查。
                // Registry 当前仍是运行时查询索引，.xjmeta 才是永久身份来源。
                const auto existing = registry.GetMeta(handle);

                if (existing && !SamePath(existing->SourcePath, pending.Path))
                {
                    spdlog::error(
                        "Asset handle {} already belongs to '{}', "
                        "cannot register '{}'.",
                        handle,
                        existing->SourcePath.string(),
                        pending.Path.string());
                    
                    ++report.Skipped;
                    ++report.Errors;
                    continue;
                }
            
                const bool alreadyRegistered = existing.has_value();
            
                XJAssetMeta registryMeta;
                registryMeta.Handle = handle;
                registryMeta.Type = pending.Type;
                registryMeta.Name = GetAssetNameFromPath(pending.Path);
                registryMeta.SourcePath = pending.Path.lexically_normal();
                registryMeta.ImportedPath = alreadyRegistered ? existing->ImportedPath : std::filesystem::path{};
            
                if (!registry.RegisterAsset(registryMeta))
                {
                    spdlog::error(
                        "Failed to register asset '{}'.",
                        pending.Path.string());
                    
                    ++report.Skipped;
                    ++report.Errors;
                    continue;
                }
            
                if (alreadyRegistered)
                    ++report.Updated;
                else
                    ++report.Added;
            }
        
            spdlog::info(
                "Asset scan complete: added={}, updated={}, "
                "metadataCreated={}, skipped={}, errors={}.",
                report.Added,
                report.Updated,
                report.MetadataCreated,
                report.Skipped,
                report.Errors);
            
            return report;
        }
}
