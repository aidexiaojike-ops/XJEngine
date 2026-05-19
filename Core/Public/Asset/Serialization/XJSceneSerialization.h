#ifndef XJ_SCENE_SERIALIZATION_H
#define XJ_SCENE_SERIALIZATION_H

#include "Asset/XJAsset.h"


namespace XJ
{
    class XJScene;
    class XJAssetManager;

    class XJSceneSerialization
    {
        public:
            // glTF → ECS Scene
            static bool LoadSceneFromGltf(XJScene* outScene,XJAssetManager* assetManager,const std::string& gltfPath);
    };
}

#endif