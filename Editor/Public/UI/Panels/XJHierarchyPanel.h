//场景树，大纲  读取 XJScene / XJEntity 层级，显示场景树，类似 UE World Outliner
#ifndef XJ_HIERARCHY_PANEL_H
#define XJ_HIERARCHY_PANEL_H

#include "UI/XJEditorSceneViewModel.h"
#include "UI/XJEditorSelection.h"
#include "UI/XJEditorUIConfig.h"
#include <string>

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
            void BeginRenameEntity(XJEditorEntityId entityId, const std::string& currentName);//开始行内改名
            void SubmitRenameEntity(XJEditorEntityId entityId);//提交行内改名请求

            XJEditorUIState& mState;
            XJEditorPanelConfig_Hierarchy* mConfig = nullptr;

            XJEditorEntityId mRenamingEntity = XJ_INVALID_EDITOR_ENTITY_ID;
            char mRenameBuffer[256] = {};
            bool mFocusRenameInput = false;
    };
}

#endif
