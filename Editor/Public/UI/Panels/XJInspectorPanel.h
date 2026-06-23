// 节点参数，细节  读取当前选中 Entity，显示并修改 Transform / Mesh / Camera / AssetRef，类似 UE Details
#ifndef XJ_INSPECTOR_PANEL_H
#define XJ_INSPECTOR_PANEL_H

#include "Asset/XJAsset.h"
#include "UI/XJEditorSceneViewModel.h"
#include "UI/XJEditorUIConfig.h"
#include "UI/XJEditorSelection.h"
#include <string>
#include <variant>

namespace XJ
{
    class XJEditorUIState;

    class XJInspectorPanel
    {
        public:
            XJInspectorPanel(XJEditorUIState& state, XJEditorPanelConfig_Inspector* config);
            ~XJInspectorPanel();

            void DrawUI();


        private:
            void DrawEntityDetails(const XJEditorEntityDetailsView& details);
            void DrawTransformComponent(const XJEditorEntityDetailsView& details);//显示 Transform 组件的 UI，允许用户修改位置、旋转、缩放等属性
            void DrawMeshRendererComponent(const XJEditorEntityDetailsView& details);//显示 MeshRenderer 组件的 UI，允许用户查看和修改网格资源、材质等属性
            void DrawCameraComponent(const XJEditorEntityDetailsView& details);//显示 Camera 组件的 UI，允许用户查看和修改摄像机的 FOV、近远裁剪面等属性
            void DrawAssetRefComponent(const XJEditorEntityDetailsView& details);//显示 AssetRef 组件的 UI，允许用户查看和修改实体引用的资产（如场景、网格等）
            void DrawAssetDetails(XJAssetHandle handle);//显示资产的详细信息，如类型、来源路径等

            bool DrawComponentFrame(const char* label, XJEditorComponentType componentType, const XJEditorEntityDetailsView& details);//组件UI
            void DrawComponentOptionsMenu(const char* label, XJEditorComponentType componentType, const XJEditorEntityDetailsView& details);//组件UI边上的三个小点打开后里面的内容
            void RequestDeleteComponent(const XJEditorEntityDetailsView& details, XJEditorComponentType componentType);//删除组件  UI
            std::string BuildComponentClipboardText(const XJEditorEntityDetailsView& details, XJEditorComponentType componentType) const;//copy组件数据
            //复制 粘贴组件里面的内容
            void CopyComponent(const XJEditorEntityDetailsView& details, XJEditorComponentType componentType);
            void PasteComponent(const XJEditorEntityDetailsView& details, XJEditorComponentType componentType);
            bool CanPasteComponent(XJEditorComponentType componentType) const;
            //设置mesh
            void RequestSetMeshRendererMesh(const XJEditorEntityDetailsView& details, XJAssetHandle meshAsset);
            void DrawMeshSelector(const XJEditorEntityDetailsView& details);
            void DrawMeshAssetDropTarget(const XJEditorEntityDetailsView& details);
            //设置材质
            void DrawMaterialSlots(const XJEditorEntityDetailsView& details);//材质插槽
            void RequestSetMeshRendererMaterial(const XJEditorEntityDetailsView& details,uint32_t slotIndex,XJAssetHandle materialAsset);//请求设置材质
            void RequestResetMeshRendererMaterial(const XJEditorEntityDetailsView& details, uint32_t slotIndex);//请求重载材质
            void DrawMaterialSelector(const XJEditorEntityDetailsView& details, const XJEditorMaterialSlotView& slot);//绘制材质选择
            //设置材质参数
            void DrawMaterialParameters(const XJEditorEntityDetailsView& details, const XJEditorMaterialSlotView& slot);//绘制材质参数
            void DrawMaterialParameterControl(const XJEditorEntityDetailsView& details, const XJEditorMaterialSlotView& slot, const XJEditorMaterialParameterView& parameter);//绘制材质参数的控制
            void RequestSetMaterialParameter(const XJEditorEntityDetailsView& details, const XJEditorMaterialSlotView& slot, const XJEditorMaterialParameterView& parameter, const XJEditorMaterialParameterValue& value);//请求设置材质参数

            void DrawAddComponentButton(const XJEditorEntityDetailsView& details);//添加组件按钮
            void RequestAddComponent(const XJEditorEntityDetailsView& details, XJEditorComponentType componentType);//准备添加组件

            XJEditorUIState& mState;
            XJEditorPanelConfig_Inspector* mConfig = nullptr;
    };
}
#endif