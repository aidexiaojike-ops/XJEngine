#include "Asset/Register/XJAssetBootstrap.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include <filesystem>
#include "spdlog/spdlog.h"
#include "Asset/Register/XJAssetRegistryScanner.h"

namespace XJ
{
    void XJAssetBootstrap::RegisterBootstrapAssets()
    {
        mAssetRegistry.RegisterAsset//来自资源文件夹的场景
        ({
            mDefaultSceneHandle,
            XJ::XJAssetType::Scene,
            "DefaultScene",
            "Resource/Scenes/Default.xjscene",
            {}
        });

        mAssetRegistry.RegisterAsset//来自资源文件夹的猴头
        ({
            mMonkeyMeshHandle,
            XJ::XJAssetType::Mesh,
            "Monkey",
            "Resource/Mesh/Monkey.glb",
            {}
        });

        mAssetRegistry.RegisterAsset//来自代码里面的cube
        ({
            mTJCubeMeshHandle,
            XJ::XJAssetType::Mesh,
            "TJCube",
            "builtin://mesh/TJCube",
            {}
        });
    }

    void XJAssetBootstrap::LoadOrCreateAssetRegistry()
    {
        const std::filesystem::path registryPath = "Resource/Config/AssetRegistry.json";

        if(std::filesystem::exists(registryPath))
        {
            if(mAssetRegistry.Load(registryPath))
            {
                spdlog::info("Loaded asset registry from {}", registryPath.string());
                RegisterBootstrapAssets();
                mAssetRegistry.Save(registryPath);   
            }
            else
            {
                spdlog::error("Failed to load asset registry from {}, starting with empty registry", registryPath.string());
                RegisterBootstrapAssets();
            }
        }
        else
        {
            RegisterBootstrapAssets();
        }

        int addedCount = XJAssetRegistryScanner::ScanResourceAssets(mAssetRegistry, "Resource");

        if (addedCount > 0 || !std::filesystem::exists(registryPath))
            mAssetRegistry.Save(registryPath);
    }
    std::shared_ptr<XJ::XJSceneAsset> XJAssetBootstrap::LoadOrCreateDefaultSceneAsset()
    {
        const std::filesystem::path scenePath = "Resource/Scenes/Default.xjscene";//读取场景json文件
        if (std::filesystem::exists(scenePath))
        {
            auto defaultScene = XJSceneAssetSerializer::LoadFromFile(scenePath);
            if (defaultScene)
                return defaultScene;
        }
         // 不存在则创建默认空场景并保存
        auto loaded = std::make_shared<XJSceneAsset>();
        loaded->mHandle = mDefaultSceneHandle;
        loaded->mType = XJAssetType::Scene;
        loaded->mName = scenePath.stem().string();
        loaded->mPath = scenePath;
        XJSceneAssetSerializer::SaveToFile(*loaded, scenePath);

        return loaded;
    }
   
}
