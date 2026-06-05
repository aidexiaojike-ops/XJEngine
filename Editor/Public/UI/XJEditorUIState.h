#ifndef XJ_EDITOR_UI_STATE_H
#define XJ_EDITOR_UI_STATE_H

#include "Edit/Mathinclude.h"
#include "Asset/XJAsset.h"
#include <filesystem>
#include <vector>

namespace XJ
{
    class XJScene;
    class XJAssetRegistry;
    class XJEntity;

    struct XJEditorUIState//编辑器 UI 的运行时状态，包含当前打开的场景、选中的实体/资产、各面板的显示状态等
    {
        XJScene* Scene = nullptr;
        XJAssetRegistry* AssetRegistry = nullptr;

        XJEntity* SelectedEntity = nullptr;
        XJAssetHandle SelectedAsset = 0;
        //四个功能UI
        bool ShowContentBrowser = true;
        bool ShowHierarchy = true;
        bool ShowInspector = true;
        bool ShowDebugConsole = true;
        //打开场景请求
        bool RequestOpenScene = false;
        std::filesystem::path RequestedScenePath;
        XJAssetHandle RequestedSceneHandle = 0;
        //拖动外部资产
        std::vector<std::filesystem::path> PendingExternalDroppedFiles;
        glm::vec2 PendingExternalDropMousePos{0.0f};
        bool HasPendingExternalDrop = false;
    };
}

#endif