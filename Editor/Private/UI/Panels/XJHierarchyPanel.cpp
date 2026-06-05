#include "UI/Panels/XJHierarchyPanel.h"

#include "UI/XJEditorUILayer.h"
#include "ECS/XJScene.h"
#include "ECS/XJEntity.h"
#include "ECS/XJNode.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJUUID.h"

#include <imgui.h>
#include <sstream>
#include <iomanip>


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

        if(!mState.Scene)
        {
            ImGui::Text("No scene loaded");
            ImGui::End();
            return;
        }

        XJNode* root = mState.Scene->XJGetRootNode();//获取场景根节点
        if(!root)
        {
            ImGui::Text("Scene has no root node");
            ImGui::End();
            return;
        }

        const auto& children = root->XJGetChildren();//获取根节点的子节点，这些子节点通常是场景中的实体
        for(XJNode* child : children)
        {
            if(XJEntity* entity = dynamic_cast<XJEntity*>(child))
            {
                DrawEntityNode(entity);
            }
        }

        ImGui::End();
    }

    void XJHierarchyPanel::DrawEntityNode(XJEntity* entity)
    {
        if(!entity)return;

        // build flags and label  生成标签和节点状态标志
        const auto& childrenEntities = entity->XJGetChildren();
        bool hasChildren = !childrenEntities.empty();//检查实体是否有子实体

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;//默认展开/收起行为
        if (hasChildren)
            flags |= ImGuiTreeNodeFlags_DefaultOpen;//如果有子实体，默认展开
        else
            flags |= ImGuiTreeNodeFlags_Leaf;//如果没有子实体，标记为叶节点

        if(mState.SelectedEntity == entity)
            flags |= ImGuiTreeNodeFlags_Selected;//如果当前实体被选中，标记为选中状态
        
        // build label with optional info 构建显示标签，包含实体名称和可选的 ID/资源来源信息
        std::string label = entity->XJGetName().empty() ? "XJUnnamed" : entity->XJGetName();

        // UUID suffix 如果启用显示 UUID，则在标签后添加实体的 UUID 后缀
        uint64_t uuid = static_cast<uint64_t>(entity->XJGetUUID());

        if (mConfig && mConfig->showEntityId)
        {
            std::stringstream visibleId;
            visibleId << " [0x" << std::hex << uuid << "]";
            label += visibleId.str();
        }

        std::stringstream imguiId;
        imguiId << "##" << std::hex << uuid;
        label += imguiId.str();

        bool opened = ImGui::TreeNodeEx(label.c_str(), flags);//创建树节点
        //chick to select 点击节点以选择实体
        if(ImGui::IsItemClicked())
        {
            mState.SelectedEntity = entity;
            mState.SelectedAsset = 0;
        }
        // right-click context menu 右键点击打开上下文菜单
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Name: %s", entity->XJGetName().c_str());
            ImGui::Text("UUID: 0x%016llX", uuid);

            bool hasMesh = entity->HasComponent<XJMeshAssetRefComponent>();
            bool hasCamera = entity->HasComponent<XJCameraComponent>();
            bool hasTransform = entity->HasComponent<XJTransformComponent>();
            bool hasSceneRef = entity->HasComponent<XJSceneAssetRefComponent>();

            ImGui::Text("Components:");
            ImGui::BulletText("Transform: %s", hasTransform ? "Yes" : "No");
            ImGui::BulletText("Mesh:      %s", hasMesh ? "Yes" : "No");
            ImGui::BulletText("Camera:    %s", hasCamera ? "Yes" : "No");
            ImGui::BulletText("SceneRef:  %s", hasSceneRef ? "Yes" : "No");
            if (mConfig && mConfig->showAssetSource && hasSceneRef)
            {
                const auto& ref = entity->GetComponent<XJSceneAssetRefComponent>();
                ImGui::Text("Source Scene: %s", ref.SourceScene.ToUri().c_str());
            }

            ImGui::EndTooltip();
        }
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Select"))
            {
                mState.SelectedEntity = entity;//选择菜单项以选择实体
                mState.SelectedAsset  = 0;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete"))
            {
                // TODO: deferred deletion
            }

            ImGui::EndPopup();
        }

        if (opened)
        {
            for (XJNode* child : childrenEntities)
            {
                if (XJEntity* childEntity = dynamic_cast<XJEntity*>(child))
                    DrawEntityNode(childEntity);
            }

            ImGui::TreePop();
        }
    }
}
