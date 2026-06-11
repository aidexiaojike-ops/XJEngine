#ifndef XJ_EDITOR_SCENE_SERVICE_H
#define XJ_EDITOR_SCENE_SERVICE_H

#include "Asset/XJAsset.h"
#include "UI/XJEditorSceneViewModel.h"
#include "UI/XJEditorSelection.h"

#include <string>
#include <vector>

namespace XJ
{
    class XJScene;
    class XJEntity;

    class XJEditorSceneService
    {
        public:
            static XJEntity* FindEntityById(XJScene& scene, XJEditorEntityId id);//ID找到ecs

            static std::vector<XJEditorEntityId> FindEntitiesUsingAsset(XJScene& scene, XJAssetHandle assetHandle);

            static XJAssetHandle GetMeshAssetFromEntity(XJScene& scene, XJEditorEntityId entityId);

            static void DeleteEntities(XJScene& scene, const std::vector<XJEditorEntityId>& entityIds);//ID是删除

            static XJEditorSceneViewModel BuildSceneViewModel(XJScene& scene);

            static XJEditorEntityDetailsView BuildEntityDetailsView(XJScene& scene, XJEditorEntityId entityId);

            static void RenameEntity(XJScene& scene, XJEditorEntityId entityId, const std::string& name);//通过ID修改名字
            static void UpdateTransform(XJScene& scene, const XJEditorUpdateTransformRequest& request);//更改位置
            static void UpdateCamera(XJScene& scene, const XJEditorUpdateCameraRequest& request);//更新摄像机
    };
}

#endif