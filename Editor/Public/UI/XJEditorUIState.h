#ifndef XJ_EDITOR_UI_STATE_H
#define XJ_EDITOR_UI_STATE_H

#include "Edit/Mathinclude.h"
#include "UI/XJEditorSceneViewModel.h"
#include "UI/XJEditorSelection.h"
#include "UI/XJEditorComponentTypes.h"

#include <filesystem>
#include <vector>


namespace XJ
{
    class XJAssetRegistry;

    struct XJEditorComponentClipboard//用作copy后粘贴数据  复制板
    {
        bool Valid = false;
        XJEditorComponentType ComponentType = XJEditorComponentType::None;  

        glm::vec3 Position{0.0f};
        glm::vec3 Rotation{0.0f};
        glm::vec3 Scale{1.0f};  

        float Fov = 60.0f;
        float NearPlane = 0.1f;
        float FarPlane = 100.0f;
    };

    struct XJEditorUIState//编辑器 UI 全局状态
    {
        XJAssetRegistry* AssetRegistry = nullptr;// // 指向资产注册表的指针

        XJEditorSelectionState Selection;// 当前选择状态
        XJEditorSceneRequestState SceneRequests;// 待处理的场景操作请求

        bool RequestSelectAssetInContentBrowser = false;
        XJAssetHandle RequestedContentBrowserAsset = 0;

        XJEditorSceneViewModel SceneView;
        XJEditorEntityDetailsView SelectedEntityDetails;
        // ---------- 面板可见性控制 ----------
        bool ShowContentBrowser = true;
        bool ShowHierarchy = true;
        bool ShowInspector = true;
        bool ShowDebugConsole = true;
        // ---------- 外部文件拖放支持 ----------
        std::vector<std::filesystem::path> PendingExternalDroppedFiles; // 待处理的外部拖入文件路径列表
        glm::vec2 PendingExternalDropMousePos{0.0f};// 拖放发生时鼠标在编辑器窗口内的坐标
        bool HasPendingExternalDrop = false; // 是否存在待处理的外部拖放事件

        XJEditorComponentClipboard ComponentClipboard;
    };


}

#endif