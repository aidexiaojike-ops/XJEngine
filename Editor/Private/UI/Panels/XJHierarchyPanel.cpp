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
    XJHierarchyPanel::XJHierarchyPanel(XJEditorUIState& state)
        : mState(state)
    {
    }

    XJHierarchyPanel::~XJHierarchyPanel()
    {
    }

    void XJHierarchyPanel::DrawUI()
    {
        if(!mState.ShowHierarchy)return;

        ImGui::Begin(mState.Scene ? "World Outliner" : "World Outliner");

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
        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_DefaultOpen;//如果有子实体，默认展开
        else
            flags |= ImGuiTreeNodeFlags_Leaf;//如果没有子实体，标记为叶节点

        if(mState.SelectedEntity == entity)
            flags |= ImGuiTreeNodeFlags_Selected;//如果当前实体被选中，标记为选中状态
        
        // build label with optional info 构建显示标签，包含实体名称和可选的 ID/资源来源信息
        std::string label = entity->XJGetName().empty() ? "XJUnnamed" : entity->XJGetName();

        // UUID suffix 如果启用显示 UUID，则在标签后添加实体的 UUID 后缀
        std::stringstream ss;
        uint64_t uuid = static_cast<uint64_t>(entity->XJGetId());
        ss << "##" << std::hex << uuid;
        label += ss.str();

        bool opened = ImGui::TreeNodeEx(label.c_str(), flags);//创建树节点
        //chick to select 点击节点以选择实体
        if(ImGui::IsItemClicked())
        {
            mState.SelectedEntity = entity;
        }
        // right-click context menu 右键点击打开上下文菜单
        if(ImGui::BeginPopupContextItem())
        {
            if(ImGui::MenuItem("Select"))
            {
                mState.SelectedEntity = entity;
            }

            ImGui::Separator();

            if(ImGui::MenuItem("Delete"))
            {
                //TODO: deferred deletion or immediate删除实体的逻辑，可能需要考虑延迟删除以避免在遍历时修改场景
            }

            ImGui::EndPopup();//结束上下文菜单

            // tooltip on hover with details 当鼠标悬停时显示工具提示，包含实体的详细信息
            if(ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Name: %s", entity->XJGetName().c_str());//显示实体名称

                uint64_t uid = static_cast<uint64_t>(entity->XJGetId());
                ImGui::Text("UUID:  0x%016llX", uid);//显示实体 UUID
                // 显示实体的组件信息，例如是否有 Mesh、Camera、Transform 等组件
                bool hasMesh = entity->HasComponent<XJMeshAssetRefComponent>();
                bool hasCamera = entity->HasComponent<XJCameraComponent>();
                bool hasTransform = entity->HasComponent<XJTransformComponent>();
                bool hasSceneRef = entity->HasComponent<XJSceneAssetRefComponent>();
                // 可以根据需要添加更多组件类型的检查
                ImGui::Text("Components:");
                ImGui::BulletText("Transform: %s", hasTransform ? "Yes" : "No");
                ImGui::BulletText("Mesh:      %s", hasMesh    ? "Yes" : "No");
                ImGui::BulletText("Camera:    %s", hasCamera  ? "Yes" : "No");
                ImGui::BulletText("SceneRef:  %s", hasSceneRef ? "Yes" : "No");
                // 可以根据需要添加更多组件信息
                ImGui::EndTooltip();
            }

            if(opened)
            {
                //recurse children递归绘制子实体
                for(XJNode* child : childrenEntities)
                {
                    if(XJEntity* childEntity = dynamic_cast<XJEntity*>(child))
                    {
                        DrawEntityNode(childEntity);
                    }
                }

                ImGui::TreePop();//结束树节点
            }
        }
    }
}