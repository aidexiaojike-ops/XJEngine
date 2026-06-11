#ifndef XJ_EDITOR_SCENE_ASSET_DROP_CONTROLLER_H
#define XJ_EDITOR_SCENE_ASSET_DROP_CONTROLLER_H

#include "Asset/XJAsset.h"
#include "Edit/Mathinclude.h"
#include "UI/XJEditorSelection.h"

#include <memory>

//处理 Content Browser asset 拖到 Scene Preview
//创建 ECS entity
//计算 drop ray 生成位置

namespace XJ
{
    class XJAssetRegistry;
    class XJEntity;
    class XJMaterial;
    class XJMesh;
    class XJSampler;
    class XJScene;
    class XJTexture;

    struct XJAssetDragPayload;
    struct XJSceneInstantiateContext;
    struct XJEditorUIState;
    class XJEditorSceneAssetDropController
    {
        public:
            bool CreateEntityFromDroppedAsset(
                XJScene& scene,
                const XJAssetDragPayload& payload,
                XJAssetRegistry& assetRegistry,
                XJSceneInstantiateContext& instantiateContext,
                XJEditorUIState& uiState,
                const std::shared_ptr<XJTexture>& defaultTexture,
                const std::shared_ptr<XJSampler>& defaultSampler);

        private:
            glm::vec3 CalculateSpawnPositionFromDropRay(
                XJScene& scene,
                const XJAssetDragPayload& payload);

            bool RaycastSceneWithinDistance(
                XJScene& scene,
                const glm::vec3& rayOrigin,
                const glm::vec3& rayDirection,
                float maxDistance,
                glm::vec3& outSpawnPosition);

            bool IntersectRaySphere(
                const glm::vec3& rayOrigin,
                const glm::vec3& rayDirection,
                const glm::vec3& center,
                float radius,
                float maxDistance,
                float& outT);
    };


}
#endif