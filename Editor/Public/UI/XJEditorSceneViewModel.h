#ifndef XJ_EDITOR_SCENE_VIEW_MODEL_H
#define XJ_EDITOR_SCENE_VIEW_MODEL_H

#include "Asset/XJAsset.h"
#include "Edit/Mathinclude.h"
#include "UI/XJEditorSelection.h"

#include <string>
#include <vector>

namespace XJ
{
    struct XJEditorEntityView
    {
        XJEditorEntityId Id = XJ_INVALID_EDITOR_ENTITY_ID;
        std::string Name;

        bool HasTransform = false;
        bool HasMesh = false;
        bool HasCamera = false;
        bool HasSceneRef = false;

        XJAssetHandle MeshAsset = 0;
        std::string SourceSceneUri;

        std::vector<XJEditorEntityView> Children;
    };

    struct XJEditorSceneViewModel
    {
        std::vector<XJEditorEntityView> RootEntities;
    };


    struct XJEditorTransformView
    {
        bool Valid = false;
        glm::vec3 Position{0.0f};
        glm::vec3 Rotation{0.0f};
        glm::vec3 Scale{1.0f};
    };

    struct XJEditorCameraView
    {
        bool Valid = false;
        float Fov = 60.0f;
        float NearPlane = 0.1f;
        float FarPlane = 100.0f;
        std::string ModeName;
    };

    struct XJEditorMeshAssetRefView
    {
        bool Valid = false;
        XJAssetHandle MeshAsset = 0;
        std::string MeshUri;
    };

    struct XJEditorSceneAssetRefView
    {
        bool Valid = false;
        std::string SourceSceneUri;
        uint64_t SourceEntity = 0;
    };

    struct XJEditorEntityDetailsView//所有的组件  id 名字  状态
    {
        bool Valid = false;

        XJEditorEntityId Id = XJ_INVALID_EDITOR_ENTITY_ID;
        std::string Name;

        XJEditorTransformView Transform;
        XJEditorCameraView Camera;
        XJEditorMeshAssetRefView Mesh;
        XJEditorSceneAssetRefView SceneRef;
    };

}

#endif