#ifndef XJ_EDITOR_SCENE_VIEW_MODEL_H
#define XJ_EDITOR_SCENE_VIEW_MODEL_H

#include "Asset/XJAsset.h"
#include "Edit/Mathinclude.h"
#include "UI/XJEditorSelection.h"
#include "UI/XJEditorMaterialTypes.h"

#include <string>
#include <vector>
#include <filesystem>

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

    struct XJEditorCameraView//摄像机窗口
    {
        bool Valid = false;
        float Fov = 60.0f;
        float NearPlane = 0.1f;
        float FarPlane = 100.0f;
        std::string ModeName;
    };

    struct XJEditorMaterialParameterView//材质参数窗口
    {
        std::string Name;
        std::string DisplayName;
        XJEditorMaterialParameterType Type = XJEditorMaterialParameterType::None;
        XJEditorMaterialParameterValue Value;

        bool Editable = true;
        bool HasRange = false;
        float Min = 0.0f;
        float Max = 1.0f;
        std::string Category;
    };

    struct XJEditorMaterialSlotView//多材质 窗口
    {
        uint32_t SlotIndex = 0;
        bool HasMaterialAsset = false;
        XJAssetHandle MaterialAsset = 0;
        std::string MaterialUri;
        std::string DisplayName;

        std::filesystem::path MaterialPath;
        std::filesystem::path ShaderPath;
        std::vector<XJEditorMaterialParameterView> Parameters;
    };

    struct XJEditorMeshAssetRefView//模型窗口
    {
        bool Valid = false;
        XJAssetHandle MeshAsset = 0;
        std::string MeshUri;

        std::vector<XJEditorMaterialSlotView> MaterialSlots;
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