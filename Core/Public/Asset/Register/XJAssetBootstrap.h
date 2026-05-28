#ifndef XJ_ASSET_BOOTSTRAP_H
#define XJ_ASSET_BOOTSTRAP_H

#include "Asset/XJAsset.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/XJSceneAsset.h"
#include <memory>
#include <filesystem>
//引导资产注册和默认场景创建。确保在引擎启动时，注册表中至少有一个默认场景资产可用。

namespace XJ
{

    class XJAssetBootstrap
    {
        private:
            XJAssetRegistry& mAssetRegistry;
            XJAssetHandle    mDefaultSceneHandle;
            XJAssetHandle    mMonkeyMeshHandle;
            
        public:
            XJAssetBootstrap(XJAssetRegistry& registry, XJAssetHandle sceneHandle, XJAssetHandle meshHandle)
                            : mAssetRegistry(registry), mDefaultSceneHandle(sceneHandle), mMonkeyMeshHandle(meshHandle)
            {}

            void RegisterBootstrapAssets();
            void LoadOrCreateAssetRegistry();
            std::shared_ptr<XJSceneAsset> LoadOrCreateDefaultSceneAsset();
       
    };
    
 
    
}


#endif