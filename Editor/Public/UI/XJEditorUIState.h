#ifndef XJ_EDITOR_UI_STATE_H
#define XJ_EDITOR_UI_STATE_H

#include "Asset/XJAsset.h"

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

        bool ShowContentBrowser = true;
        bool ShowHierarchy = true;
        bool ShowInspector = true;
        bool ShowDebugConsole = true;
    };
}

#endif