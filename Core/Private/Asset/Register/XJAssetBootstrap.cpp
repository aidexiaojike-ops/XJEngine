#include "Asset/Register/XJAssetBootstrap.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include <filesystem>
#include "spdlog/spdlog.h"

namespace XJ
{
    void XJAssetBootstrap::RegisterBootstrapAssets()
    {
        mAssetRegistry.RegisterAsset
        ({
            mDefaultSceneHandle,
            XJ::XJAssetType::Scene,
            "DefaultScene",
            "Resource/Scenes/Default.xjscene",
            {}
        });

        mAssetRegistry.RegisterAsset
        ({
            mMonkeyMeshHandle,
            XJ::XJAssetType::Mesh,
            "Monkey",
            "Resource/Mesh/Monkey.glb",
            {}
        });
    }

    void XJAssetBootstrap::LoadOrCreateAssetRegistry()
    {
        const std::filesystem::path registryPath = "Resource/Config/AssetRegistry.json";
        if (std::filesystem::exists(registryPath) && mAssetRegistry.Load(registryPath))
            return;

        RegisterBootstrapAssets();
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
        XJSceneAssetSerializer::SaveToFile(*loaded, scenePath);

        return loaded;
    }
   
}