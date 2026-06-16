#ifndef XJ_EDITOR_SELECTION_H
#define XJ_EDITOR_SELECTION_H

#include "Asset/XJAsset.h"
#include "Edit/Mathinclude.h"
#include "UI/XJEditorComponentTypes.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace XJ
{
    using XJEditorEntityId = uint64_t;// 编辑器实体唯一标识符类型
    constexpr XJEditorEntityId XJ_INVALID_EDITOR_ENTITY_ID = 0;// 无效的实体 ID 常量

    struct XJEditorCreateEmptyEntityRequest//创建一个空实体
    {
        bool AsChild = false;
        XJEditorEntityId ParentEntity = XJ_INVALID_EDITOR_ENTITY_ID;
        std::string Name = "Empty Entity";
    };

    struct XJEditorAddComponentRequest//添加组件功能
    {
        XJEditorEntityId EntityId = XJ_INVALID_EDITOR_ENTITY_ID;
        XJEditorComponentType ComponentType = XJEditorComponentType::None;
    };
    
    struct XJEditorDeleteComponentRequest//删除组件
    {
        XJEditorEntityId EntityId = XJ_INVALID_EDITOR_ENTITY_ID;
        XJEditorComponentType ComponentType = XJEditorComponentType::None;
    };

    struct XJEditorSelectionState//编辑器选择状态
    {
        XJEditorEntityId SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;// 当前选中的实体 ID
        XJAssetHandle SelectedAsset = 0;// 当前选中的资产句柄

        std::unordered_set<XJEditorEntityId> HighlightedEntities;// 当前高亮的实体集合
    };

    struct XJEditorRenameEntityRequest//重命名实体请求
    {
        XJEditorEntityId EntityId = XJ_INVALID_EDITOR_ENTITY_ID;
        std::string Name;//新名字
    };

    struct XJEditorUpdateTransformRequest//更新实体变换
    {
        XJEditorEntityId EntityId = XJ_INVALID_EDITOR_ENTITY_ID;
        glm::vec3 Position{0.0f};
        glm::vec3 Rotation{0.0f};
        glm::vec3 Scale{1.0f};
    };

    struct XJEditorUpdateCameraRequest//更新摄像机组件参数请求
    {
        XJEditorEntityId EntityId = XJ_INVALID_EDITOR_ENTITY_ID;
        float Fov = 60.0f;
        float NearPlane = 0.1f;
        float FarPlane = 100.0f;
    };

    struct XJEditorSetMeshRendererMeshRequest//设置渲染mesh 
    {
        XJEditorEntityId EntityId = XJ_INVALID_EDITOR_ENTITY_ID;
        XJAssetHandle MeshAsset = 0;
    };
 


    struct XJEditorSceneRequestState//编辑器场景请求状态  这些请求由 UI 层设置，由控制器在合适的时机统一处理
    {
        bool RequestSaveScene = false;//// 是否请求保存当前场景

        std::vector<XJEditorEntityId> RequestDeleteEntities;//请求删除实体ID列表

        bool RequestOpenScene = false;// 是否请求打开新场景
        std::filesystem::path RequestedScenePath;// 要打开的场景文件路径
        XJAssetHandle RequestedSceneHandle = 0;// 要打开的场景资产句柄

        XJAssetHandle RequestFindEntitiesUsingAsset = 0;// 请求查找场景中使用指定资产

        bool RequestRenameEntity = false; // 是否请求重命名实体
        XJEditorRenameEntityRequest RenameEntity;//参数

        bool RequestUpdateTransform = false;// 是否请求更新实体变换
        XJEditorUpdateTransformRequest UpdateTransform;///参数

        bool RequestUpdateCamera = false;// 是否请求更新摄像机参数
        XJEditorUpdateCameraRequest UpdateCamera;//参数

        bool RequestCreateEmptyEntity = false;//是否请求创建一个空实体
        XJEditorCreateEmptyEntityRequest CreateEmptyEntity;

        bool RequestAddComponent = false;//是否请求添加组件
        XJEditorAddComponentRequest AddComponent;

        bool RequestDeleteComponent = false;//是否请求删除组件
        XJEditorDeleteComponentRequest DeleteComponent;

        bool RequestSetMeshRendererMesh = false;//是否请求设置渲染mesh
        XJEditorSetMeshRendererMeshRequest SetMeshRendererMesh;
    };


}

#endif

/*
之前 UI 是这样：
HierarchyPanel -> XJScene -> XJNode -> XJEntity -> Component
InspectorPanel -> XJScene -> XJEntity -> Component
这样的问题是：

UI 直接知道 ECS 类型。
UI 可以随便读/改 ECS。
删除实体、打开场景、刷新 UI 时容易出现悬空指针。
UI 代码会被 ECS 细节污染，比如 HasComponent<T>()、GetComponent<T>()、XJNode children。
后面如果 ECS 内部结构变了，UI 全部要跟着改。
现在变成：

main/service -> 从 ECS 生成 XJEditorSceneViewModel
HierarchyPanel -> 只读 ViewModel
InspectorPanel -> 只读 DetailsView
UI 修改 -> 写 SceneRequests
main/service -> 处理请求，改 ECS
所以 XJEditorSceneViewModel 的核心作用是：

ECS 数据 -> UI 专用数据快照


UI 不再 include ECS。
UI 不持有 XJEntity*，减少悬空指针。
Hierarchy/Inspector 只管显示，不管 ECS 如何存储。
ECS 修改全部集中到 XJEditorSceneService / main。
以后可以把 UI 放到别的线程、做搜索过滤、缓存、undo/redo，都更容易。
删除实体后，UI 只会看到下一帧新快照，不会拿旧指针继续画。
*/