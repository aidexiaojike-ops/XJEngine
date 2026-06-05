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
        XJUUID UUID = 0;
        glm::vec3 Position {0.0f};
        glm::vec3 Rotation {0.0f};
        glm::vec3 Scale {1.0f};
    };

    struct XJSceneMeshRendererData
    {
        XJUUID UUID = 0;
        XJAssetRef Mesh;
        std::vector<XJAssetRef> Materials;
    };

    struct XJSceneCameraData
    {
        XJUUID UUID = 0;
        bool Enabled = false;
        float Fov = 60.0f;
        float NearClip = 0.1f;
        float FarClip = 1000.0f;
        bool Primary = false;
    };

    struct XJSceneLightData
    {
        XJUUID UUID = 0;
        bool Enabled = false;
        int Type = 0; // 0 directional, 1 point, 2 spot
        glm::vec3 Color {1.0f};
        float Intensity = 1.0f;
    };

    struct XJSceneEntityData//场景 资产的entity
    {
        XJUUID UUID = 0;
        std::string Type = "Entity";
        std::string Name;

        XJUUID Parent = 0;
        std::vector<XJUUID> Children;

        bool HasTransform = false;
        bool HasMeshRenderer = false;
        bool HasCamera = false;
        bool HasLight = false;

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

            XJSceneEntityData* FindEntity(XJUUID uuid);//根据 UUID 查找实体
            const XJSceneEntityData* FindEntity(XJUUID uuid) const;
    };
    
}


#endif