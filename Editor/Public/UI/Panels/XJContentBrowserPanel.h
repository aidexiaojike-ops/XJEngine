//资产  读取 AssetRegistry，显示资产列表，类似 UE 内容浏览器
#ifndef XJ_CONTENT_BROWSER_PANEL_H
#define XJ_CONTENT_BROWSER_PANEL_H

#include "Asset/XJAsset.h"
#include "UI/XJEditorUIConfig.h"


namespace XJ
{
    class XJEditorUIState;

    class XJContentBrowserPanel
    {
        public:
            XJContentBrowserPanel(XJEditorUIState& state, XJEditorPanelConfig_ContentBrowser* config);
            ~XJContentBrowserPanel();


            void DrawUI(); 
        private:
            XJEditorUIState& mState;

            void DrawToolbar();//显示工具栏，包含搜索框和过滤选项
            void DrawAssetTable();//显示资产列表，支持按类型、名称等属性进行排序和过滤
            void HandleSelection(XJAssetHandle handle);//处理用户选择资产的逻辑，例如更新编辑器状态中的 SelectedAsset，或在 Inspector 面板中显示选中资产的详细信息
            static const char* AssetTypeToString(XJAssetType type);//将资产类型枚举转换为字符串，供 UI 显示使用

            XJEditorPanelConfig_ContentBrowser* mConfig = nullptr;
    };
}


#endif