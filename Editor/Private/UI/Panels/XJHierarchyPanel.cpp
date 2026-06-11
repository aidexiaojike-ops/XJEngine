#include "UI/Panels/XJHierarchyPanel.h"

#include "UI/XJEditorUIState.h"

#include <imgui.h>
#include <iomanip>
#include <sstream>



namespace XJ
{
    XJHierarchyPanel::XJHierarchyPanel(XJEditorUIState& state, XJEditorPanelConfig_Hierarchy* config)
        : mState(state), mConfig(config)
    {
    }

    XJHierarchyPanel::~XJHierarchyPanel()
    {
    }

    void XJHierarchyPanel::DrawUI()
    {
        if(!mState.ShowHierarchy)return;

        const char* title = mConfig ? mConfig->title.c_str() : "World Outliner";
        ImGui::Begin(title);

        if(mState.SceneView.RootEntities.empty())
        {
            ImGui::Text("No scene loaded");
            ImGui::End();
            return;
        }

        for (const XJEditorEntityView& entity : mState.SceneView.RootEntities)
            DrawEntityNode(entity);

        ImGui::End();
    }

    void XJHierarchyPanel::DrawEntityNode(const XJEditorEntityView& entity)
    {
        if (entity.Id == XJ_INVALID_EDITOR_ENTITY_ID)
            return;

        bool hasChildren = !entity.Children.empty();//检查实体是否有子实体

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;//默认展开/收起行为
        if (hasChildren)
            flags |= ImGuiTreeNodeFlags_DefaultOpen;//如果有子实体，默认展开
        else
            flags |= ImGuiTreeNodeFlags_Leaf;//如果没有子实体，标记为叶节点


        bool selected = mState.Selection.SelectedEntity == entity.Id;
        bool highlighted = mState.Selection.HighlightedEntities.count(entity.Id) > 0;

        if(selected || highlighted)
            flags |= ImGuiTreeNodeFlags_Selected;//如果当前实体被选中，标记为选中状态
        
        // build label with optional info 构建显示标签，包含实体名称和可选的 ID/资源来源信息
        std::string label = entity.Name.empty() ? "XJUnnamed" : entity.Name;

        if (mConfig && mConfig->showEntityId)
        {
            std::stringstream visibleId;
            visibleId << " [0x" << std::hex << entity.Id << "]";
            label += visibleId.str();
        }

        std::stringstream imguiId;
        imguiId << "##" << std::hex << entity.Id;
        label += imguiId.str();


        bool opened = ImGui::TreeNodeEx(label.c_str(), flags);//创建树节点
        //chick to select 点击节点以选择实体
        if (ImGui::IsItemClicked())
        {
            mState.Selection.SelectedEntity = entity.Id;
            mState.Selection.SelectedAsset = 0;
            mState.Selection.HighlightedEntities.clear();
        }
        // right-click context menu 右键点击打开上下文菜单
         if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Name: %s", entity.Name.c_str());
            ImGui::Text("UUID: 0x%016llX", static_cast<uint64_t>(entity.Id));

            ImGui::Text("Components:");
            ImGui::BulletText("Transform: %s", entity.HasTransform ? "Yes" : "No");
            ImGui::BulletText("Mesh:      %s", entity.HasMesh ? "Yes" : "No");
            ImGui::BulletText("Camera:    %s", entity.HasCamera ? "Yes" : "No");
            ImGui::BulletText("SceneRef:  %s", entity.HasSceneRef ? "Yes" : "No");

            if (mConfig && mConfig->showAssetSource && entity.HasSceneRef)
                ImGui::Text("Source Scene: %s", entity.SourceSceneUri.c_str());

            ImGui::EndTooltip();
        }
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Select Mesh Asset", nullptr, false, entity.MeshAsset != 0))
            {
                mState.Selection.SelectedAsset = entity.MeshAsset;
                mState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
                mState.Selection.HighlightedEntities.clear();

                mState.RequestSelectAssetInContentBrowser = true;
                mState.RequestedContentBrowserAsset = entity.MeshAsset;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete From Scene"))
            {
                if (!mState.Selection.HighlightedEntities.empty() &&
                    mState.Selection.HighlightedEntities.count(entity.Id) > 0)
                {
                    for (XJEditorEntityId highlightedId : mState.Selection.HighlightedEntities)
                        mState.SceneRequests.RequestDeleteEntities.push_back(highlightedId);
                }
                else
                {
                    mState.SceneRequests.RequestDeleteEntities.push_back(entity.Id);
                }
            }

            ImGui::EndPopup();
        }

        if (opened)
        {
            for (const XJEditorEntityView& child : entity.Children)
                DrawEntityNode(child);

            ImGui::TreePop();
        }
    }


}
