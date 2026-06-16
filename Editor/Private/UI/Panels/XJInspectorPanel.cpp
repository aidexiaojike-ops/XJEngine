#include "UI/Panels/XJInspectorPanel.h"
#include "Asset/XJAssetRegistry.h"
#include "UI/XJEditorUIState.h"
//#include "UI/XJEditorDragPayload.h"
#include "UI/XJEditorAssetDragPayload.h"

#include <imgui.h>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <string>



namespace XJ
{
    static const char* AssetTypeToString(XJAssetType type)
    {
        switch (type)
        {
            case XJAssetType::None: return "None";
            case XJAssetType::Mesh: return "Mesh";
            case XJAssetType::Texture: return "Texture";
            case XJAssetType::Material: return "Material";
            case XJAssetType::Scene: return "Scene";
            default: return "Unknown";
        }
    }
    XJInspectorPanel::XJInspectorPanel(XJEditorUIState& state, XJEditorPanelConfig_Inspector* config)
        : mState(state), mConfig(config)
    {
    }

    XJInspectorPanel::~XJInspectorPanel()
    {
    }

    void XJInspectorPanel::DrawUI()
    {
        if(!mState.ShowInspector)
            return;

        const char* title = mConfig ? mConfig->title.c_str() : "Details";
        ImGui::Begin(title);

        if (mState.SelectedEntityDetails.Valid)
        {
            DrawEntityDetails(mState.SelectedEntityDetails);
            ImGui::End();
            return;
        }

        if (mState.Selection.SelectedAsset != 0)
        {
            DrawAssetDetails(mState.Selection.SelectedAsset);
            ImGui::End();
            return;
        }

        ImGui::Text("No entity selected"); 
        ImGui::End();
    }

    void XJInspectorPanel::DrawEntityDetails(const XJEditorEntityDetailsView& details)
    {
        if(ImGui::CollapsingHeader("Entity", ImGuiTreeNodeFlags_DefaultOpen))
        {
            char nameBuffer[255] = {};
            std::strncpy(nameBuffer, details.Name.empty()?"XJUnnamed" : details.Name.c_str(), sizeof(nameBuffer) -1);

            if(ImGui::InputText("Name", nameBuffer,sizeof(nameBuffer)))
            {
                mState.SceneRequests.RequestRenameEntity = true;
                mState.SceneRequests.RenameEntity.EntityId = details.Id;
                mState.SceneRequests.RenameEntity.Name = nameBuffer;
            }
            ImGui::LabelText("UUID", "0x%016llX", static_cast<uint64_t>(details.Id));
        }

        bool showTransform = !mConfig || mConfig->showTransform;
        bool showMeshRenderer = !mConfig || mConfig->showMeshRenderer;
        bool showCamera = !mConfig || mConfig->showCamera;
        bool showAssetRefs = !mConfig || mConfig->showAssetRefs;

        if (showTransform && details.Transform.Valid)
            DrawTransformComponent(details);

        if (showCamera && details.Camera.Valid)
            DrawCameraComponent(details);

        if (showMeshRenderer && details.Mesh.Valid)
            DrawMeshRendererComponent(details);

        if (showAssetRefs && details.SceneRef.Valid)
            DrawAssetRefComponent(details);

        DrawAddComponentButton(details);
    }
    void XJInspectorPanel::DrawTransformComponent(const XJEditorEntityDetailsView& details)
    {
        if (!DrawComponentFrame("Transform", XJEditorComponentType::Transform, details))
            return;

        glm::vec3 position = details.Transform.Position;
        glm::vec3 rotation = details.Transform.Rotation;
        glm::vec3 scale = details.Transform.Scale;

        bool changed = false;

        // helper: drag float3 with reset button  显示位置、旋转、缩放的输入框，允许用户修改这些属性，并提供重置按钮
        auto DragVec3 = [&](const char* label, glm::vec3& value, float speed)
        {
            /*Position  X [   ]  Y [   ]  Z [   ]
              Rotation  X [   ]  Y [   ]  Z [   ]
              Scale     X [   ]  Y [   ]  Z [   ]*/
            float arr[3] = { value.x, value.y, value.z };
            bool dragChanged = false;
            
            ImGui::PushID(label);//为每个属性创建唯一 ID，避免 ImGui 内部冲突
            ImGui::TextUnformatted(label);//显示属性标签
            ImGui::SameLine(90.0F);

            float fullWidth = ImGui::GetContentRegionAvail().x;
            float itemWidth = (fullWidth - ImGui::GetStyle().ItemSpacing.x * 4.0f) / 3.0f;

            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(itemWidth);
            dragChanged |= ImGui::DragFloat("##X", &arr[0], speed);

            ImGui::SameLine();
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(itemWidth);
            dragChanged |= ImGui::DragFloat("##Y", &arr[1], speed);

            ImGui::SameLine();
            ImGui::TextUnformatted("Z");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(itemWidth);
            dragChanged |= ImGui::DragFloat("##Z", &arr[2], speed);


            if (dragChanged)
                value = glm::vec3(arr[0], arr[1], arr[2]);

            ImGui::PopID();
            
            return dragChanged;
        };
        // 使用不同的颜色区分位置、旋转、缩放属性，增强视觉层次感
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.27f, 0.27f, 1.0f));
        changed |= DragVec3("Position", position, 0.1f);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.27f, 0.69f, 0.27f, 1.0f));
        changed |= DragVec3("Rotation", rotation, 1.0f);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.27f, 0.54f, 0.94f, 1.0f));
        changed |= DragVec3("Scale", scale, 0.01f);
        ImGui::PopStyleColor();

        if(changed)
        {
            mState.SceneRequests.RequestUpdateTransform = true;
            mState.SceneRequests.UpdateTransform.EntityId = details.Id;
            mState.SceneRequests.UpdateTransform.Position = position;
            mState.SceneRequests.UpdateTransform.Rotation = rotation;
            mState.SceneRequests.UpdateTransform.Scale = scale;//如果用户修改了 Transform 属性，更新实体的模型矩阵
        }

        ImGui::TreePop();

    }

    void XJInspectorPanel::DrawCameraComponent(const XJEditorEntityDetailsView& details)
    {
        if (!DrawComponentFrame("Camera", XJEditorComponentType::Camera, details))
            return;

        float fov = details.Camera.Fov;
        float nearPlane = details.Camera.NearPlane;
        float farPlane = details.Camera.FarPlane;

        //show FOV, near/far planes, mode 显示摄像机的 FOV、近远裁剪面和当前模式，允许用户修改这些属性
        bool changed = false;

        changed |= ImGui::DragFloat("FOV", &fov, 1.0f, 1.0f, 179.0f);
        changed |= ImGui::DragFloat("Near", &nearPlane, 0.01f, 0.001f, 1000.0f, "%.3f");
        changed |= ImGui::DragFloat("Far", &farPlane, 1.0f, 0.1f, 10000.0f, "%.1f");

        ImGui::LabelText("Mode", "%s", details.Camera.ModeName.c_str());

        if (changed)
        {
            mState.SceneRequests.RequestUpdateCamera = true;
            mState.SceneRequests.UpdateCamera.EntityId = details.Id;
            mState.SceneRequests.UpdateCamera.Fov = fov;
            mState.SceneRequests.UpdateCamera.NearPlane = nearPlane;
            mState.SceneRequests.UpdateCamera.FarPlane = farPlane;
        }

        ImGui::TreePop();
    }

    void XJInspectorPanel::DrawAssetRefComponent(const XJEditorEntityDetailsView& details)
    {
        if (!DrawComponentFrame("Scene Asset Ref", XJEditorComponentType::SceneAssetRef, details))
            return;

        ImGui::LabelText("Source Scene URI", "%s", details.SceneRef.SourceSceneUri.c_str());
        ImGui::LabelText("Source Entity UUID", "0x%016llX", details.SceneRef.SourceEntity);

        ImGui::TreePop();
    }

    void XJInspectorPanel::DrawAssetDetails(XJAssetHandle handle)
    {
        if(!mState.AssetRegistry)
        {
            ImGui::Text("No Asset Registry");
            return;
        }

        auto metaOpt = mState.AssetRegistry->GetMeta(handle);
        if(!metaOpt.has_value())
        {
            ImGui::Text("Asset not found: 0x%016llX", static_cast<uint64_t>(handle));
            return;
        }   

        const auto& meta = metaOpt.value();

        if(ImGui::CollapsingHeader("Asset", ImGuiTreeNodeFlags_DefaultOpen))
        {
            //ImGui::LabelText("Name", "%s", meta.Name.c_str());
            //ImGui::LabelText("Handle", "0x%016llX", static_cast<uint64_t>(meta.Handle));
            //ImGui::LabelText("Type", "%d", static_cast<int>(meta.Type));
            //ImGui::LabelText("Source", "%s", meta.SourcePath.string().c_str());
            //ImGui::LabelText("Imported", "%s", meta.ImportedPath.string().c_str());

            ImGui::LabelText("Name", "%s", meta.Name.c_str());
            ImGui::LabelText("Handle", "0x%016llX", static_cast<uint64_t>(meta.Handle));
            ImGui::LabelText("Type", "%s", AssetTypeToString(meta.Type));
            ImGui::LabelText("Source", "%s", meta.SourcePath.string().c_str());
            ImGui::LabelText("Imported", "%s", meta.ImportedPath.string().c_str());
        }
    }


    void XJInspectorPanel::DrawAddComponentButton(const XJEditorEntityDetailsView& details)
    {
        ImGui::Separator();

        if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup("AddComponentPopup");
        
        if(ImGui::BeginPopup("AddComponentPopup"))
        {
            bool hasAnyAvailable = false;

            if(!details.Transform.Valid)
            {
                hasAnyAvailable = true;
                if(ImGui::MenuItem("Transform"))
                    RequestAddComponent(details, XJEditorComponentType::Transform);
            }

            if (!details.Camera.Valid)
            {
                hasAnyAvailable = true;
                if (ImGui::MenuItem("Camera"))
                    RequestAddComponent(details, XJEditorComponentType::Camera);
            }

            if (!details.Mesh.Valid)
            {
                hasAnyAvailable = true;
                if (ImGui::MenuItem("Mesh Renderer"))
                    RequestAddComponent(details, XJEditorComponentType::MeshRenderer);
            }

            if (!hasAnyAvailable)
                ImGui::TextDisabled("No components available");

            

            ImGui::EndPopup();
        }
    }

    void XJInspectorPanel::RequestAddComponent(const XJEditorEntityDetailsView& details, XJEditorComponentType componentType)
    {
        if (details.Id == XJ_INVALID_EDITOR_ENTITY_ID)
            return;

        mState.SceneRequests.RequestAddComponent = true;
        mState.SceneRequests.AddComponent.EntityId = details.Id;
        mState.SceneRequests.AddComponent.ComponentType = componentType;
    }

    //组件 统一外框函数
    bool XJInspectorPanel::DrawComponentFrame(const char* label, XJEditorComponentType componentType, const XJEditorEntityDetailsView& details)
    {
        ImGui::PushID(label);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | 
                                   ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;

        bool open = ImGui::TreeNodeEx("##ComponentFrame", flags, "%s", label);
        

        float buttonSize = ImGui::GetFrameHeight();
        float buttonX = ImGui::GetWindowContentRegionMax().x - buttonSize - ImGui::GetStyle().FramePadding.x;
        ImGui::SameLine(buttonX);

        if (ImGui::SmallButton("...##ComponentOptionsButton"))
            ImGui::OpenPopup("ComponentOptions");
        
        DrawComponentOptionsMenu(label, componentType, details);

        ImGui::PopID();

        return open;
    }

    void XJInspectorPanel::DrawComponentOptionsMenu(const char* label, XJEditorComponentType componentType, const XJEditorEntityDetailsView& details)
    {
        if (!ImGui::BeginPopup("ComponentOptions"))
            return;

        if (ImGui::MenuItem("Copy Component"))
            CopyComponent(details, componentType);

        ImGui::Separator();

        bool canPaste = CanPasteComponent(componentType);
        if (ImGui::MenuItem("Paste Component", nullptr, false, canPaste))
            PasteComponent(details, componentType);

        ImGui::Separator();

        if (ImGui::MenuItem("Delete Component"))
            RequestDeleteComponent(details, componentType);

        ImGui::EndPopup();
    }

    void XJInspectorPanel::RequestDeleteComponent(const XJEditorEntityDetailsView& details, XJEditorComponentType componentType)
    {
        if(details.Id == XJ_INVALID_EDITOR_ENTITY_ID)
            return;

        mState.SceneRequests.RequestDeleteComponent = true;
        mState.SceneRequests.DeleteComponent.EntityId = details.Id;
        mState.SceneRequests.DeleteComponent.ComponentType = componentType;
    }

    std::string XJInspectorPanel::BuildComponentClipboardText(const XJEditorEntityDetailsView& details, XJEditorComponentType componentType) const
    {
        char buffer[1024] = {};

        switch (componentType)
        {
            case XJEditorComponentType::Transform:
            {
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "Transform\nPosition: %.3f %.3f %.3f\nRotation: %.3f %.3f %.3f\nScale: %.3f %.3f %.3f",
                    details.Transform.Position.x,
                    details.Transform.Position.y,
                    details.Transform.Position.z,
                    details.Transform.Rotation.x,
                    details.Transform.Rotation.y,
                    details.Transform.Rotation.z,
                    details.Transform.Scale.x,
                    details.Transform.Scale.y,
                    details.Transform.Scale.z);
                return buffer;
            }

            case XJEditorComponentType::Camera:
            {
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "Camera\nFOV: %.3f\nNear: %.3f\nFar: %.3f\nMode: %s",
                    details.Camera.Fov,
                    details.Camera.NearPlane,
                    details.Camera.FarPlane,
                    details.Camera.ModeName.c_str());
                return buffer;
            }

            case XJEditorComponentType::MeshRenderer:
            {
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "Mesh Renderer\nMesh Asset: 0x%016llX\nMesh URI: %s",
                    static_cast<unsigned long long>(details.Mesh.MeshAsset),
                    details.Mesh.MeshUri.c_str());
                return buffer;
            }

            case XJEditorComponentType::SceneAssetRef:
            {
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "Scene Asset Ref\nSource Scene URI: %s\nSource Entity UUID: 0x%016llX",
                    details.SceneRef.SourceSceneUri.c_str(),
                    static_cast<unsigned long long>(details.SceneRef.SourceEntity));
                return buffer;
            }

            default:
                return "Unknown Component";
        }
    }
    void XJInspectorPanel::CopyComponent(const XJEditorEntityDetailsView& details, XJEditorComponentType componentType)
    {
        std::string text = BuildComponentClipboardText(details, componentType);
        ImGui::SetClipboardText(text.c_str());

        mState.ComponentClipboard = {};
        mState.ComponentClipboard.Valid = true;
        mState.ComponentClipboard.ComponentType = componentType;

        switch (componentType)
        {
            case XJEditorComponentType::Transform:
            {
                mState.ComponentClipboard.Position = details.Transform.Position;
                mState.ComponentClipboard.Rotation = details.Transform.Rotation;
                mState.ComponentClipboard.Scale = details.Transform.Scale;
                break;
            }

            case XJEditorComponentType::Camera:
            {
                mState.ComponentClipboard.Fov = details.Camera.Fov;
                mState.ComponentClipboard.NearPlane = details.Camera.NearPlane;
                mState.ComponentClipboard.FarPlane = details.Camera.FarPlane;
                break;
            }

            default:
                break;
        }
    }

    bool XJInspectorPanel::CanPasteComponent(XJEditorComponentType componentType) const
    {
        if (!mState.ComponentClipboard.Valid)
            return false;

        if (mState.ComponentClipboard.ComponentType != componentType)
            return false;

        return componentType == XJEditorComponentType::Transform ||
               componentType == XJEditorComponentType::Camera;
    }

    void XJInspectorPanel::PasteComponent(const XJEditorEntityDetailsView& details, XJEditorComponentType componentType)
    {
        if (details.Id == XJ_INVALID_EDITOR_ENTITY_ID)
            return;

        if (!CanPasteComponent(componentType))
            return;

        switch (componentType)
        {
            case XJEditorComponentType::Transform:
            {
                if (!details.Transform.Valid)
                    return;

                mState.SceneRequests.RequestUpdateTransform = true;
                mState.SceneRequests.UpdateTransform.EntityId = details.Id;
                mState.SceneRequests.UpdateTransform.Position = mState.ComponentClipboard.Position;
                mState.SceneRequests.UpdateTransform.Rotation = mState.ComponentClipboard.Rotation;
                mState.SceneRequests.UpdateTransform.Scale = mState.ComponentClipboard.Scale;
                break;
            }

            case XJEditorComponentType::Camera:
            {
                if (!details.Camera.Valid)
                    return;

                mState.SceneRequests.RequestUpdateCamera = true;
                mState.SceneRequests.UpdateCamera.EntityId = details.Id;
                mState.SceneRequests.UpdateCamera.Fov = mState.ComponentClipboard.Fov;
                mState.SceneRequests.UpdateCamera.NearPlane = mState.ComponentClipboard.NearPlane;
                mState.SceneRequests.UpdateCamera.FarPlane = mState.ComponentClipboard.FarPlane;
                break;
            }

            default:
                break;
        }
    }

    void XJInspectorPanel::DrawMeshRendererComponent(const XJEditorEntityDetailsView& details)
    {
        if (!DrawComponentFrame("Mesh Renderer", XJEditorComponentType::MeshRenderer, details))
            return;

        ImGui::LabelText("Static Mesh", "0x%016llX", static_cast<uint64_t>(details.Mesh.MeshAsset));//显示实体引用的网格资源的句柄，格式化为 16 位十六进制数
        ImGui::LabelText("Mesh URI", "%s", details.Mesh.MeshUri.c_str());//显示实体引用的网格资源的句柄，格式化为 16 位十六进制数

/*
        std::string uri = meshRef.Mesh.ToUri();//显示网格资源的 URI，提供更友好的资源标识
        ImGui::LabelText("Mesh URI", "%s", uri.c_str());

         // TODO: Materials array display (from XJMaterialAssetRefComponent) 显示实体引用的材质资源，可能需要从另一个组件（如 XJMaterialAssetRefComponent）获取材质信息，并以列表形式显示
        if(entity->HasComponent<XJMaterialAssetRefComponent>())
        {
            auto& materialRef = entity->GetComponent<XJMaterialAssetRefComponent>();
            ImGui::Separator();//分割线
            ImGui::TextUnformatted("Materials:");//材质列表标题
            if(materialRef.Materials.empty())
            {
                ImGui::TextDisabled("No materials assigned.");//如果没有材质，显示提示信息
            }
            else
            {
                for(size_t i = 0; i < materialRef.Materials.size(); ++i)
                {
                    const auto& mat = materialRef.Materials[i];
                    std::string matUri = mat.ToUri();
                    ImGui::BulletText("Material %zu: %s", i, matUri.c_str());//显示每个材质的 URI，使用项目符号列表格式
                }
            }
        }
*/
        DrawMeshSelector(details);
        DrawMeshAssetDropTarget(details);

        ImGui::TreePop();
    }
   
    void XJInspectorPanel::RequestSetMeshRendererMesh(const XJEditorEntityDetailsView& details, XJAssetHandle meshAsset)
    {
        if (details.Id == XJ_INVALID_EDITOR_ENTITY_ID || meshAsset == 0)
            return;

        mState.SceneRequests.RequestSetMeshRendererMesh = true;
        mState.SceneRequests.SetMeshRendererMesh.EntityId = details.Id;
        mState.SceneRequests.SetMeshRendererMesh.MeshAsset = meshAsset;
    }

    void XJInspectorPanel::DrawMeshSelector(const XJEditorEntityDetailsView& details)
    {
        if (!mState.AssetRegistry)
        {
            ImGui::TextDisabled("No Asset Registry");
            return;
        }

        ImGui::Spacing();

        if (ImGui::Button("Select Mesh", ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup("SelectMeshPopup");

        if (ImGui::BeginPopup("SelectMeshPopup"))
        {
            static char searchBuffer[128] = {};
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));

            ImGui::Separator();

            const std::string search = searchBuffer;

            bool found = false;
            for (const auto& [handle, meta] : mState.AssetRegistry->XJGetAllMetas())
            {
                if (meta.Type != XJAssetType::Mesh)
                    continue;

                if (!search.empty() && meta.Name.find(search) == std::string::npos)
                    continue;

                found = true;

                bool selected = details.Mesh.MeshAsset == handle;
                if (ImGui::Selectable(meta.Name.c_str(), selected))
                {
                    RequestSetMeshRendererMesh(details, handle);
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", meta.SourcePath.string().c_str());
            }

            if (!found)
                ImGui::TextDisabled("No mesh assets found");

            ImGui::EndPopup();
        }
    }

    void XJInspectorPanel::DrawMeshAssetDropTarget(const XJEditorEntityDetailsView& details)
    {
        ImGui::Spacing();
    
        ImGui::Button("Drop Mesh Asset Here", ImVec2(-1.0f, 0.0f));
    
        if (!ImGui::BeginDragDropTarget())
            return;
    
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(XJ_ASSET_PAYLOAD_NAME))
        {
            if (payload->DataSize == sizeof(XJEditorAssetDragPayload))
            {
                const auto* assetPayload = static_cast<const XJEditorAssetDragPayload*>(payload->Data);
            
                if (assetPayload &&
                    assetPayload->Type == XJAssetType::Mesh &&
                    assetPayload->Handle != 0)
                {
                    RequestSetMeshRendererMesh(details, assetPayload->Handle);
                }
            }
        }
    
        ImGui::EndDragDropTarget();
    }
}
