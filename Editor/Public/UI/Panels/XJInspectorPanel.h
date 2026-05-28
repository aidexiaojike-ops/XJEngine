// 节点参数，细节  读取当前选中 Entity，显示并修改 Transform / Mesh / Camera / AssetRef，类似 UE Details
#ifndef XJ_INSPECTOR_PANEL_H
#define XJ_INSPECTOR_PANEL_H

namespace XJ
{
    class XJEditorUIState;
    class XJEntity;

    class XJInspectorPanel
    {
        public:
            XJInspectorPanel(XJEditorUIState& state);
            ~XJInspectorPanel();

            void SetState(XJEditorUIState* state) const { if (state) mState = *state;};
            void DrawUI();

        private:
            void DrawTransformComponent(XJEntity* entity);//显示 Transform 组件的 UI，允许用户修改位置、旋转、缩放等属性
            void DrawMeshRendererComponent(XJEntity* entity);//显示 MeshRenderer 组件的 UI，允许用户查看和修改网格资源、材质等属性
            void DrawCameraComponent(XJEntity* entity);//显示 Camera 组件的 UI，允许用户查看和修改摄像机的 FOV、近远裁剪面等属性
            void DrawAssetRefComponent(XJEntity* entity);//显示 AssetRef 组件的 UI，允许用户查看和修改实体引用的资产（如场景、网格等）

            XJEditorUIState& mState;
    };
}
#endif