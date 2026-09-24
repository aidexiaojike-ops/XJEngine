#ifndef XJ_SCENE_INSTANTIATOR_H
#define XJ_SCENE_INSTANTIATOR_H

#include "Asset/XJSceneAsset.h"
#include "Asset/Loader/XJScriptAssetLoader.h"
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
    class XJSurfaceMaterial;

    enum class XJSceneMaterialPolicy
    {
        // 编辑场景可以复用全局资产材质缓存。
        SharedAsset,
        // RuntimeScene 只共享纹理等资源，材质参数实例独立。
        IsolatedScene
    };

    struct XJSceneInstantiateContext//场景实例化上下文，包含加载场景时需要的各种资源和缓存
    {
        XJAssetRegistry* Registry = nullptr;//列表
        XJAssetRef SourceScene;//序列
        std::shared_ptr<XJTexture> DefaultTexture;
        std::shared_ptr<XJSampler> DefaultSampler;
        std::unordered_map<XJAssetHandle, std::shared_ptr<XJMesh>> MeshCache;
        std::unordered_map<XJUUID, XJEntity*> EntityMap;

        XJSceneMaterialPolicy MaterialPolicy = XJSceneMaterialPolicy::SharedAsset;
        // Instantiate 返回 false 时，调用方据此判断是否需要恢复旧 Scene。
        bool TargetSceneModified = false;
        // Play clone 开启；编辑场景加载允许保留编译失败脚本供用户修复。
        bool RequireCompiledScripts = false;
        // 仅属于本次实例化 Scene，不进入全局材质缓存。
        std::unordered_map<XJAssetHandle, std::shared_ptr<XJSurfaceMaterial>> SceneMaterialCache;
        std::unordered_map<XJAssetHandle, XJScriptAssetCacheEntry> ScriptCache;
        std::shared_ptr<XJSurfaceMaterial> SceneDefaultMaterial;
    };

    class XJSceneInstantiator
    {
        public:
            static bool Instantiate(const XJSceneAsset& asset, XJScene& outScene, XJSceneInstantiateContext* context = nullptr);
            static XJEntity* FindInstantiatedEntity(const XJSceneInstantiateContext& ctx, XJUUID id);//根据场景实例化上下文和实体 ID 查找已经实例化的运行时实体
        private:
            static bool ValidateAsset(const XJSceneAsset& asset, XJSceneInstantiateContext& context);
            static XJEntity* CreateEntity(const XJSceneEntityData& data, XJScene& scene, XJSceneInstantiateContext& context);//根据场景实体数据创建一个运行时实体，并应用其组件数据
            static void ApplyTransform(const XJSceneEntityData& data, XJEntity& entity);//将场景实体数据中的变换信息应用到运行时实体的变换组件上
            static bool ApplyMeshRenderer(const XJSceneEntityData& data, XJEntity& entity, XJSceneInstantiateContext& context);//   将场景实体数据中的网格渲染信息应用到运行时实体的网格渲染组件上，包括加载网格资源和设置材质
            static void ApplyCamera(const XJSceneEntityData& data, XJEntity& entity);//将场景实体数据中的摄像机信息应用到运行时实体的摄像机组件上
            static bool ApplyHierarchy(const XJSceneAsset& asset, XJSceneInstantiateContext& context);//根据场景资产中的父子关系数据，建立运行时实体之间的层级关系
            static void ApplyLight(const XJSceneEntityData& data, XJEntity& entity);//将场景实体数据中的灯光信息应用到运行时实体的灯光组件上
            static bool ApplyScript(const XJSceneEntityData& data, XJEntity& entity);
    };
}

#endif
