#ifndef XJ_ASSET_BOOTSTRAP_H
#define XJ_ASSET_BOOTSTRAP_H

#include "Asset/XJAsset.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/XJSceneAsset.h"
#include <memory>
#include <filesystem>
#include <utility>
//引导资产注册和默认场景创建。确保在引擎启动时，注册表中至少有一个默认场景资产可用。

namespace XJ
{

    class XJAssetBootstrap
    {
        private:
            XJAssetRegistry& mAssetRegistry;//所有资产
            XJAssetHandle    mDefaultSceneHandle;//场景
            XJAssetHandle    mMonkeyMeshHandle;//猴头
            XJAssetHandle    mTJCubeMeshHandle;//cube
            std::filesystem::path mResourceRoot;
            std::filesystem::path mRegistryPath;
            std::filesystem::path mDefaultScenePath;
            bool mBootstrapAssetsValid = true;
            
        public:
            XJAssetBootstrap(
                XJAssetRegistry& registry,
                XJAssetHandle sceneHandle,
                XJAssetHandle meshHandle,
                XJAssetHandle cubeMeshHandle,
                std::filesystem::path resourceRoot,
                std::filesystem::path registryPath,
                std::filesystem::path defaultScenePath)
                : mAssetRegistry(registry),
                  mDefaultSceneHandle(sceneHandle),
                  mMonkeyMeshHandle(meshHandle),
                  mTJCubeMeshHandle(cubeMeshHandle),
                  mResourceRoot(std::move(resourceRoot)),
                  mRegistryPath(std::move(registryPath)),
                  mDefaultScenePath(std::move(defaultScenePath))
            {}

            void RegisterBootstrapAssets();
            bool LoadOrCreateAssetRegistry();
            std::shared_ptr<XJSceneAsset> LoadOrCreateDefaultSceneAsset();
       
    };
    
 
    
}


#endif
