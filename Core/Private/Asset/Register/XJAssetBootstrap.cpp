#include "Asset/Register/XJAssetBootstrap.h"
#include "Asset/Metadata/XJAssetMetadata.h"
#include "Asset/Metadata/XJAssetMetadataSerializer.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "spdlog/spdlog.h"
#include "Asset/Register/XJAssetRegistryScanner.h"

namespace XJ
{
    namespace
    {
        std::string NormalizeBootstrapPath(const std::filesystem::path& path)
        {
            const std::string raw = path.generic_string();
            if (raw.rfind("builtin:", 0) == 0)
                return raw;

            std::error_code ec;
            auto absolute = std::filesystem::absolute(path, ec);
            std::string normalized = (ec ? path : absolute).lexically_normal().generic_string();
#ifdef _WIN32
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
#endif
            return normalized;
        }

        bool RegisterBootstrapAssetChecked(XJAssetRegistry& registry, const XJAssetMeta& expected)
        {
            const auto existingHandle = registry.GetMeta(expected.Handle);
            if (existingHandle)
            {
                if (existingHandle->Type != expected.Type ||
                    NormalizeBootstrapPath(existingHandle->SourcePath) != NormalizeBootstrapPath(expected.SourcePath))
                {
                    spdlog::error(
                        "Bootstrap asset handle conflict: handle {} owns '{}' instead of '{}'.",
                        expected.Handle,
                        existingHandle->SourcePath.string(),
                        expected.SourcePath.string());
                    return false;
                }
            }

            const std::string expectedPath = NormalizeBootstrapPath(expected.SourcePath);
            for (const auto& [handle, meta] : registry.XJGetAllMetas())
            {
                if (handle != expected.Handle && NormalizeBootstrapPath(meta.SourcePath) == expectedPath)
                {
                    spdlog::error(
                        "Bootstrap asset path conflict: '{}' is owned by handle {}, expected {}.",
                        expected.SourcePath.string(),
                        handle,
                        expected.Handle);
                    return false;
                }
            }

            if (!expected.SourcePath.generic_string().starts_with("builtin:"))
            {
                const auto metadata = XJAssetMetadataSerializer::Load(expected.SourcePath);
                if (metadata.Succeeded() &&
                    (metadata.Metadata->Handle != expected.Handle || metadata.Metadata->Type != expected.Type))
                {
                    spdlog::error(
                        "Bootstrap asset metadata conflict: '{}' is owned by handle {}.",
                        expected.SourcePath.string(),
                        metadata.Metadata->Handle);
                    return false;
                }
                if (!metadata.Succeeded() && metadata.Status != XJAssetMetadataLoadStatus::NotFound)
                {
                    spdlog::error(
                        "Bootstrap asset metadata is unreadable '{}': {}",
                        expected.SourcePath.string(),
                        metadata.Error);
                    return false;
                }
            }

            return existingHandle.has_value() || registry.RegisterAsset(expected);
        }

        bool EnsureDefaultSceneMetadata(const std::filesystem::path& scenePath, XJAssetHandle handle)
        {
            const auto loaded = XJAssetMetadataSerializer::Load(scenePath);
            if (loaded.Succeeded())
            {
                if (loaded.Metadata->Handle == handle && loaded.Metadata->Type == XJAssetType::Scene)
                    return true;

                spdlog::error("Default scene metadata ownership conflict: {}", scenePath.string());
                return false;
            }

            if (loaded.Status != XJAssetMetadataLoadStatus::NotFound)
            {
                spdlog::error("Default scene metadata is invalid '{}': {}", scenePath.string(), loaded.Error);
                return false;
            }

            XJAssetMetadata metadata;
            metadata.Handle = handle;
            metadata.Type = XJAssetType::Scene;
            metadata.Importer = "SceneSerializer";
            metadata.ImporterVersion = 1;

            std::string error;
            if (!XJAssetMetadataSerializer::Save(scenePath, metadata, &error))
            {
                spdlog::error("Failed to create default scene metadata '{}': {}", scenePath.string(), error);
                return false;
            }
            return true;
        }
    }

    void XJAssetBootstrap::RegisterBootstrapAssets()
    {
        mBootstrapAssetsValid = true;
        mBootstrapAssetsValid &= RegisterBootstrapAssetChecked(mAssetRegistry, XJAssetMeta{ //来自资源文件夹的场景
            mDefaultSceneHandle,
            XJ::XJAssetType::Scene,
            "DefaultScene",
            mDefaultScenePath,
            {}
        });

        mBootstrapAssetsValid &= RegisterBootstrapAssetChecked(mAssetRegistry, XJAssetMeta{ //来自资源文件夹的猴头
            mMonkeyMeshHandle,
            XJ::XJAssetType::Mesh,
            "Monkey",
            mResourceRoot / "Mesh/Monkey.glb",
            {}
        });

        mBootstrapAssetsValid &= RegisterBootstrapAssetChecked(mAssetRegistry, XJAssetMeta{ //来自代码里面的cube
            mTJCubeMeshHandle,
            XJ::XJAssetType::Mesh,
            "TJCube",
            "builtin://mesh/TJCube",
            {}
        });
    }

    bool XJAssetBootstrap::LoadOrCreateAssetRegistry()
    {
        if(std::filesystem::exists(mRegistryPath))
        {
            if(mAssetRegistry.Load(mRegistryPath))
            {
                spdlog::info("Loaded asset registry from {}", mRegistryPath.string());
                RegisterBootstrapAssets();
                if (!mBootstrapAssetsValid)
                {
                    spdlog::error("Asset bootstrap aborted because fixed asset ownership conflicts were found.");
                    return false;
                }
            }
            else
            {
                spdlog::error("Failed to load asset registry from {}, starting with empty registry", mRegistryPath.string());
                RegisterBootstrapAssets();
                if (!mBootstrapAssetsValid)
                    return false;
            }
        }
        else
        {
            RegisterBootstrapAssets();
            if (!mBootstrapAssetsValid)
                return false;
        }

        const auto registryBeforeScan = mAssetRegistry.XJGetAllMetas();
        const XJAssetRegistryScanReport report =
            XJAssetRegistryScanner::ScanResourceAssetsDetailed(mAssetRegistry, mResourceRoot);

        if (report.FilesystemErrors > 0)
            return false;

        // Scanner 已构造并提交完整快照；无论 added 是否为 0，都要保存移动、删除和隔离结果。
        if (!mAssetRegistry.Save(mRegistryPath))
        {
            spdlog::error("Failed to save scanned asset registry: {}", mRegistryPath.string());
            if (!mAssetRegistry.ReplaceAssets(registryBeforeScan))
                spdlog::critical("Failed to restore registry snapshot after bootstrap save failure.");
            return false;
        }

        return report.Succeeded();
    }
    std::shared_ptr<XJ::XJSceneAsset> XJAssetBootstrap::LoadOrCreateDefaultSceneAsset()
    {
        const std::filesystem::path& scenePath = mDefaultScenePath;//项目源场景路径
        if (std::filesystem::exists(scenePath))
        {
            auto defaultScene = XJSceneAssetSerializer::LoadFromFile(scenePath);
            if (defaultScene && EnsureDefaultSceneMetadata(scenePath, mDefaultSceneHandle))
                return defaultScene;
            return nullptr;
        }

        // 默认场景创建事务：场景落盘后立即创建匹配的 sidecar。
        auto loaded = std::make_shared<XJSceneAsset>();
        loaded->mHandle = mDefaultSceneHandle;
        loaded->mType = XJAssetType::Scene;
        loaded->mName = scenePath.stem().string();
        loaded->mPath = scenePath;
        if (!XJSceneAssetSerializer::SaveToFile(*loaded, scenePath))
        {
            std::error_code ec;
            std::filesystem::remove(scenePath, ec);
            return nullptr;
        }

        if (!EnsureDefaultSceneMetadata(scenePath, mDefaultSceneHandle))
        {
            std::error_code ec;
            std::filesystem::remove(scenePath, ec);
            if (ec)
                spdlog::critical("Default scene rollback failed '{}': {}", scenePath.string(), ec.message());
            return nullptr;
        }

        return loaded;
    }
   
}
