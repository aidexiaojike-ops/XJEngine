#ifndef XJ_SCENE_INSTANTIATOR_H
#define XJ_SCENE_INSTANTIATOR_H

#include "Asset/XJSceneAsset.h"
#include <memory>
#include <unordered_map>

namespace XJ
{
    class XJScene;
    class XJEntity;
    class XJAssetRegistry;
    class XJMesh;
    class XJTexture;
    class XJSampler;

    struct XJSceneInstantiateContext
    {
        XJAssetRegistry* Registry = nullptr;
        XJAssetRef SourceScene;
        std::shared_ptr<XJTexture> DefaultTexture;
        std::shared_ptr<XJSampler> DefaultSampler;
        std::unordered_map<XJAssetHandle, std::shared_ptr<XJMesh>> MeshCache;
        std::unordered_map<XJUUID, XJEntity*> EntityMap;
    };

    class XJSceneInstantiator
    {
        public:
            static bool Instantiate(const XJSceneAsset& asset, XJScene& outScene, XJSceneInstantiateContext* context = nullptr);
            static XJEntity* FindInstantiatedEntity(const XJSceneInstantiateContext& ctx, XJUUID id);
        private:
            static XJEntity* CreateEntity(const XJSceneEntityData& data, XJScene& scene, XJSceneInstantiateContext& context);
            static void ApplyTransform(const XJSceneEntityData& data, XJEntity& entity);
            static void ApplyMeshRenderer(const XJSceneEntityData& data, XJEntity& entity, XJSceneInstantiateContext& context);
            static void ApplyCamera(const XJSceneEntityData& data, XJEntity& entity);
            static void ApplyHierarchy(const XJSceneAsset& asset, XJSceneInstantiateContext& context);
    };
}

#endif
