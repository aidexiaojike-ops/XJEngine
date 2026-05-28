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

    struct XJSceneInstantiateContext//场景实例化上下文，包含加载场景时需要的各种资源和缓存
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
            static XJEntity* FindInstantiatedEntity(const XJSceneInstantiateContext& ctx, XJUUID id);//根据场景实例化上下文和实体 ID 查找已经实例化的运行时实体
        private:
            static XJEntity* CreateEntity(const XJSceneEntityData& data, XJScene& scene, XJSceneInstantiateContext& context);//根据场景实体数据创建一个运行时实体，并应用其组件数据
            static void ApplyTransform(const XJSceneEntityData& data, XJEntity& entity);//将场景实体数据中的变换信息应用到运行时实体的变换组件上
            static void ApplyMeshRenderer(const XJSceneEntityData& data, XJEntity& entity, XJSceneInstantiateContext& context);//   将场景实体数据中的网格渲染信息应用到运行时实体的网格渲染组件上，包括加载网格资源和设置材质
            static void ApplyCamera(const XJSceneEntityData& data, XJEntity& entity);//将场景实体数据中的摄像机信息应用到运行时实体的摄像机组件上
            static void ApplyHierarchy(const XJSceneAsset& asset, XJSceneInstantiateContext& context);//根据场景资产中的父子关系数据，建立运行时实体之间的层级关系
    };
}

#endif
