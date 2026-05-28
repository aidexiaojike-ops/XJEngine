//场景树，大纲  读取 XJScene / XJEntity 层级，显示场景树，类似 UE World Outliner
#ifndef XJ_HIERARCHY_PANEL_H
#define XJ_HIERARCHY_PANEL_H

namespace XJ
{
    class XJEditorUIState;
    class XJEntity;

    class XJHierarchyPanel
    {
        public:
            XJHierarchyPanel(XJEditorUIState& state);
            ~XJHierarchyPanel();

            void SetState(XJEditorUIState* state) const { if (state) mState = *state;};

            void DrawUI();

        private:
            void DrawEntityNode(XJEntity* entity);
            XJEditorUIState& mState;
    };
}

#endif