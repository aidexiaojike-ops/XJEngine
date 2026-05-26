#ifndef XJ_SCENE_ASSET_H
#define XJ_SCENE_ASSET_H
//磁盘上的 .xjscene 数据结构。它不是 ECS，不包含运行时指针。
//资产数据结构  FindEntity对UUID找到实体

#include "Asset/XJAsset.h"
#include "Asset/XJAssetRef.h"
#include "ECS/XJUUID.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace XJ
{
    struct XJSceneTransformData
    {
        glm::vec3 Position {0.0f};
        glm::vec3 Rotation {0.0f};
        glm::vec3 Scale {1.0f};
    };

    struct XJSceneMeshRendererData
    {
        XJAssetRef Mesh;
        std::vector<XJAssetRef> Materials;
    };

    struct XJSceneCameraData
    {
        bool Enabled = false;
        float Fov = 60.0f;
        float NearClip = 0.1f;
        float FarClip = 1000.0f;
        bool Primary = false;
    };

    struct XJSceneLightData
    {
        bool Enabled = false;
        int Type = 0; // 0 directional, 1 point, 2 spot
        glm::vec3 Color {1.0f};
        float Intensity = 1.0f;
    };

    struct XJSceneEntityData//场景 资产的entity
    {
        XJUUID Id = 0;
        std::string Name;

        XJUUID Parent = 0;
        std::vector<XJUUID> Children;

        XJSceneTransformData Transform;
        XJSceneMeshRendererData MeshRenderer;
        XJSceneCameraData Camera;
        XJSceneLightData Light;
    };

    class XJSceneAsset : public XJAsset
    {
        public:
            XJSceneAsset(){ mType = XJAssetType::Scene;}

            std::vector<XJSceneEntityData> Entities;//场景里面所有的资产

            XJSceneEntityData* FindEntity(XJUUID id);//根据 UUID 查找实体
            const XJSceneEntityData* FindEntity(XJUUID id) const;
    };
    
}


#endif