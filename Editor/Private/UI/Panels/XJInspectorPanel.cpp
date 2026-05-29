#include "UI/Panels/XJInspectorPanel.h"

#include "UI/XJEditorUILayer.h"
#include "ECS/XJEntity.h"
#include "ECS/XJUUID.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"

#include <imgui.h>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace XJ
{
    XJInspectorPanel::XJInspectorPanel(XJEditorUIState& state, XJEditorPanelConfig_Inspector* config)
        : mState(state), mConfig(config)
    {
    }

    XJInspectorPanel::~XJInspectorPanel()
    {
    }

    void XJInspectorPanel::DrawUI()
    {
        if(!mState.ShowInspector)return;

        const char* title = mConfig ? mConfig->title.c_str() : "Details";
        ImGui::Begin(title);

        XJEntity* entity = mState.SelectedEntity;

        if(!entity)
        {
            ImGui::Text("No entity selected");
            ImGui::End();
            return;
        }
        if(!entity->IsValid())
        {
            ImGui::Text("Selected entity is not valid");
            ImGui::End();
            return;
        }
        // === Entity Header === 显示实体名称和 UUID
        if(ImGui::CollapsingHeader("Entity", ImGuiTreeNodeFlags_DefaultOpen))
        {
            char nameBuffer[256] = {};//如果实体没有名称，显示 "XJUnnamed"

            std::string currentName = entity->XJGetName();
            // std::strncpy(nameBuffer, currentName.c_str(), sizeof(nameBuffer) - 1);
            std::strncpy(nameBuffer, currentName.empty() ? "XJUnnamed" : currentName.c_str(), sizeof(nameBuffer) - 1);//显示实体名称的输入框，允许用户修改名称
            if(ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
            {
                entity->XJSetName(nameBuffer);
            }
            uint64_t uuid = static_cast<uint64_t>(entity->XJGetId());

            ImGui::LabelText("UUID",  "0x%016llX", uuid);//显示实体的 UUID，格式化为 16 位十六进制数
        }
        // === Transform ===
        bool showTransform = !mConfig || mConfig->showTransform;
        bool showMeshRenderer = !mConfig || mConfig->showMeshRenderer;
        bool showCamera = !mConfig || mConfig->showCamera;
        bool showAssetRefs = !mConfig || mConfig->showAssetRefs;

        if (showTransform && entity->HasComponent<XJTransformComponent>())
        {
            DrawTransformComponent(entity);
        }
        else if (showTransform)
        {
            if (ImGui::CollapsingHeader("Transform"))
                ImGui::TextDisabled("Entity does not have a Transform component.");
        }
         // === Camera ===
        if (showCamera && entity->HasComponent<XJCameraComponent>())
        {
            DrawCameraComponent(entity);
        }
         // === Mesh Renderer ===
        if (showMeshRenderer && entity->HasComponent<XJMeshAssetRefComponent>())
        {
            DrawMeshRendererComponent(entity);
        }
         // === Scene Asset Ref ===
        if (showAssetRefs && entity->HasComponent<XJSceneAssetRefComponent>())
        {
            DrawAssetRefComponent(entity);
        }
        ImGui::End();
    }

    void XJInspectorPanel::DrawTransformComponent(XJEntity* entity)
    {
        if(!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& transform = entity->GetComponent<XJTransformComponent>();
        bool changed = false;

        // helper: drag float3 with reset button  显示位置、旋转、缩放的输入框，允许用户修改这些属性，并提供重置按钮
        auto DragVec3 = [&](const char* label, glm::vec3& value, float speed = 0.1f, float reset = 0.0f)
        {
            float arr[3] = { value.x, value.y, value.z };
            bool dragChanged = false;
            
            ImGui::PushID(label);//为每个属性创建唯一 ID，避免 ImGui 内部冲突
            ImGui::TextUnformatted(label);//显示属性标签

            ImGui::PushItemWidth(-1);//让输入框占满剩余空间
            dragChanged |= ImGui::DragFloat("##X", &arr[0], speed);
            dragChanged |= ImGui::DragFloat("##Y", &arr[1], speed);
            dragChanged |= ImGui::DragFloat("##Z", &arr[2], speed);
            ImGui::PopItemWidth();//恢复默认输入框宽度

            ImGui::PopID();

            if(dragChanged)
            {
                value = glm::vec3(arr[0], arr[1], arr[2]);
                //changed = true;
            }

            return dragChanged;
        };
        // 使用不同的颜色区分位置、旋转、缩放属性，增强视觉层次感
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.27f, 0.27f, 1.0f));
        changed |= DragVec3("Position", transform.position, 0.1f);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.27f, 0.69f, 0.27f, 1.0f));
        changed |= DragVec3("Rotation", transform.rotation, 1.0f);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.27f, 0.54f, 0.94f, 1.0f));
        changed |= DragVec3("Scale", transform.scale, 0.01f, 1.0f);
        ImGui::PopStyleColor();

        if(changed)
        {
            transform.UpdateModelMatrix();//如果用户修改了 Transform 属性，更新实体的模型矩阵
        }

    }

    void XJInspectorPanel::DrawMeshRendererComponent(XJEntity* entity)
    {
         if (!ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& meshRef = entity->GetComponent<XJMeshAssetRefComponent>();

        ImGui::LabelText("Mesh Asset", "0x%016llX", static_cast<uint64_t>(meshRef.Mesh.Handle));//显示实体引用的网格资源的句柄，格式化为 16 位十六进制数
        ImGui::LabelText("Mesh Type", "0x%016llX", static_cast<int>(meshRef.Mesh.Type));//显示实体引用的网格资源的句柄，格式化为 16 位十六进制数
        

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
    }

    void XJInspectorPanel::DrawCameraComponent(XJEntity* entity)
    {
        if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& camera = entity->GetComponent<XJCameraComponent>();
        //show FOV, near/far planes, mode 显示摄像机的 FOV、近远裁剪面和当前模式，允许用户修改这些属性
        float fov = camera.XJGetFov();//显示摄像机的 FOV，允许用户修改 FOV，范围从 1 到 179 度，步长为 1 度
        if(ImGui::DragFloat("FOV", &fov, 1.0f, 1.0f, 179.0f))
        {
            camera.XJSetFov(fov);
        }

        float nearPlane = camera.XJGetNear();//显示摄像机的近裁剪面，允许用户修改近裁剪面，范围从 0.001 到 1000，步长为 0.01
        if (ImGui::DragFloat("Near", &nearPlane, 0.01f, 0.001f, 1000.0f, "%.3f"))
            camera.XJSetNear(nearPlane);

        float farPlane = camera.XJGetFar();//显示摄像机的远裁剪面，允许用户修改远裁剪面，范围从 0.1 到 10000，步长为 0.1
        if (ImGui::DragFloat("Far", &farPlane, 1.0f, 0.1f, 10000.0f, "%.1f"))
            camera.XJSetFar(farPlane);

        const char* modeNames[] = { "Orbit", "Free" };//摄像机模式的名称数组，按照 CameraMode 枚举的顺序排列
        int currentMode = static_cast<int>(camera.XJGetCameraMode());
        ImGui::LabelText("Mode", "%s", modeNames[currentMode]);
    }

    void XJInspectorPanel::DrawAssetRefComponent(XJEntity* entity)
    {
        if (!ImGui::CollapsingHeader("Scene Asset Ref"))
            return;

        auto& sceneRef = entity->GetComponent<XJSceneAssetRefComponent>();

        ImGui::LabelText("Source Scene URI", "%s", sceneRef.SourceScene.ToUri().c_str());
        uint64_t srcEntity = static_cast<uint64_t>(sceneRef.SourceEntity);
        ImGui::LabelText("Source Entity UUID", "0x%016llX", srcEntity);
    }
}