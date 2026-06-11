//场景树，大纲  读取 XJScene / XJEntity 层级，显示场景树，类似 UE World Outliner
#ifndef XJ_HIERARCHY_PANEL_H
#define XJ_HIERARCHY_PANEL_H

#include "UI/XJEditorSceneViewModel.h"
#include "UI/XJEditorSelection.h"
#include "UI/XJEditorUIConfig.h"

namespace XJ
{
    class XJEditorUIState;

    class XJHierarchyPanel
    {
    public:
        XJHierarchyPanel(XJEditorUIState& state, XJEditorPanelConfig_Hierarchy* config);
        ~XJHierarchyPanel();

        void DrawUI();

    private:
        void DrawEntityNode(const XJEditorEntityView& entity);

        XJEditorUIState& mState;
        XJEditorPanelConfig_Hierarchy* mConfig = nullptr;
    };
}

#endif