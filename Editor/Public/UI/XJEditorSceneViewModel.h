#ifndef XJ_EDITOR_SCENE_VIEW_MODEL_H
#define XJ_EDITOR_SCENE_VIEW_MODEL_H

#include "Asset/XJAsset.h"
#include <glm/vec3.hpp>
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
        bool HasLight = false;
        bool HasScript = false;

        XJAssetHandle MeshAsset = 0;
        std::string SourceSceneUri;

        std::vector<XJEditorEntityView> Children;
    };

    struct XJEditorSceneViewModel
    {
        std::vector<XJEditorEntityView> RootEntities;
    };

    struct XJEditorWorldTransformView
    {
        bool Valid = false;

        glm::dvec3 Position{0.0};
        glm::dvec3 Rotation{0.0};
        glm::dvec3 Scale{1.0};

        std::string Error;
    };


    struct XJEditorTransformView
    {
        // 表示实体具有 Transform。
        // World 读取失败不影响 Local 数据可用性。
        bool Valid = false;

        // 以下三个字段始终表示 Local。
        // 保留现有组件剪贴板和旧调用端语义。
        glm::vec3 Position{0.0f};
        glm::vec3 Rotation{0.0f};
        glm::vec3 Scale{1.0f};

        XJEditorWorldTransformView World;
    };

    struct XJEditorCameraView//摄像机窗口
    {
        bool Valid = false;
        float Fov = 60.0f;
        float NearPlane = 0.1f;
        float FarPlane = 100.0f;
        std::string ModeName;
    };

    struct XJEditorLightView//灯光窗口
    {
        bool Valid = false;
        int Type = 0;                 // 0方向/1点/2聚光
        bool Enabled = true;
        glm::vec3 Color{1.0f, 1.0f, 1.0f};
        float Intensity = 1.0f;
        float Range = 10.0f;
        float InnerAngleDegrees = 20.0f;
        float OuterAngleDegrees = 30.0f;
    };


    struct XJEditorMaterialParameterView//材质参数窗口
    {
        std::string Name;
        std::string DisplayName;
        XJEditorMaterialParameterType Type = XJEditorMaterialParameterType::None;
        XJEditorMaterialParameterValue Value;

        bool IsOverride = false;
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

    struct XJEditorScriptSlotView
    {
        uint64_t SlotId = 0;
        bool Enabled = true;
        XJAssetHandle ScriptAsset = 0;
        std::string ScriptUri;
        std::string DisplayName;
        uint32_t OverrideCount = 0;
    };

    struct XJEditorScriptView
    {
        bool Valid = false;
        std::vector<XJEditorScriptSlotView> Slots;
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
        XJEditorLightView Light;   
        XJEditorScriptView Script;
    };

}

#endif
