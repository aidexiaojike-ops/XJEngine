#ifndef XJ_EDITOR_SCENE_SERVICE_H
#define XJ_EDITOR_SCENE_SERVICE_H

#include "Asset/XJAsset.h"
#include "UI/XJEditorSceneViewModel.h"
#include "UI/XJEditorSelection.h"
#include "UI/XJEditorComponentTypes.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace XJ
{
    class XJScene;
    class XJEntity;
    class XJAssetRegistry;
    struct XJSceneInstantiateContext;
    class XJTexture;
    class XJSampler;

    class XJEditorSceneService
    {
        public:
            using ShouldExposeEntityCallback = std::function<bool(XJEditorEntityId id)>;//是否隐藏实体

            static XJEntity* FindEntityById(XJScene& scene, XJEditorEntityId id);//ID找到ecs
            static std::vector<XJEditorEntityId> FindEntitiesUsingAsset(XJScene& scene, XJAssetHandle assetHandle);
            static XJAssetHandle GetMeshAssetFromEntity(XJScene& scene, XJEditorEntityId entityId);
            static void DeleteEntities(XJScene& scene, const std::vector<XJEditorEntityId>& entityIds);//ID是删除

            static XJEditorSceneViewModel BuildSceneViewModel(XJScene& scene, const ShouldExposeEntityCallback& shouldExposeEntity = {});//显示模型

            static XJEditorEntityDetailsView BuildEntityDetailsView(XJScene& scene, XJEditorEntityId entityId, const ShouldExposeEntityCallback& shouldExposeEntity);

            static void RenameEntity(XJScene& scene, XJEditorEntityId entityId, const std::string& name);//通过ID修改名字
            static void UpdateTransform(XJScene& scene, const XJEditorUpdateTransformRequest& request);//更改位置
            static void UpdateCamera(XJScene& scene, const XJEditorUpdateCameraRequest& request);//更新摄像机

            static XJEditorEntityId CreateEmptyEntity(XJScene& scene, const std::string& name, XJEditorEntityId parentId);//创建一个空的实体
            static bool AddComponent(XJScene& scene, XJEditorEntityId entityId, XJEditorComponentType componentType);//添加组件
            static bool DeleteComponent(XJScene& scene, XJEditorEntityId entityId, XJEditorComponentType componentType);//删除组件
            static bool AddMeshRendererComponent(XJScene& scene, XJEditorEntityId entityId, XJAssetHandle defaultMeshAsset, XJAssetRegistry& assetRegistry, XJSceneInstantiateContext& instantiateContext, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler);//添加mesh组件
            static bool SetMeshRendererMesh(XJScene& scene, XJEditorEntityId entityId, XJAssetHandle meshAsset, XJAssetRegistry& assetRegistry, XJSceneInstantiateContext& instantiateContext, const std::shared_ptr<XJTexture>& defaultTexture, const std::shared_ptr<XJSampler>& defaultSampler);//设置mesh

    };
}

#endif