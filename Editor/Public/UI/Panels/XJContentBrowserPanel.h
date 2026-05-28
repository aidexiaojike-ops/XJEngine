//资产  读取 AssetRegistry，显示资产列表，类似 UE 内容浏览器
#ifndef XJ_CONTENT_BROWSER_PANEL_H
#define XJ_CONTENT_BROWSER_PANEL_H


namespace XJ
{
    class XJEditorUIState;

    class XJContentBrowserPanel
    {
        public:
            XJContentBrowserPanel(XJEditorUIState& state);
            ~XJContentBrowserPanel();

            void SetState(XJEditorUIState* state) const { if (state) mState = *state;};
            void DrawUI();

        private:
            XJEditorUIState& mState;
    };
}


#endif