#include "UI/Viewports/XJScenePreview.h"

#include "ECS/XJEntity.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include <glm/gtc/matrix_inverse.hpp> 

#include "UI/XJEditorAssetDragPayload.h"
#include "UI/XJEditorDragPayload.h"
#include <cmath>


namespace XJ
{
    bool XJScenePreview::Init(XJRenderContext* renderContext)
    {
        return mSurface.Init(renderContext, mSettings.mWidth, mSettings.mHeight, true);
    }

    void XJScenePreview::Shutdown()
    {
        mSurface.Shutdown();
    }

    void XJScenePreview::PrepareBeforeRender()
    {
        mSurface.PrepareBeforeRender();
    }

    void XJScenePreview::PostRender()
    {
        mSurface.PostRender();
    }

    bool XJScenePreview::Render(VkCommandBuffer cmd)
    {
        mSurface.SetCamera(mPreviewCamera);

        if (!mSurface.BeginRender(cmd))
            return false;

        mSurface.RenderMaterialSystem(cmd);
        mSurface.EndRender(cmd);
        return true;
    }

    ImTextureID XJScenePreview::GetViewportTextureID() const
    {
        return mSurface.GetTextureID();
    }

    bool XJScenePreview::IsViewportTextureReady() const
    {
        return mSurface.IsTextureReady();
    }

    void XJScenePreview::OnViewportResized(uint32_t width, uint32_t height)
    {
        mSurface.Resize(width, height);
    }

    bool XJScenePreview::CalculateDropPositionFromViewportRay(const ImVec2& imageMin, const ImVec2& imageSize, glm::vec3& outOrigin, glm::vec3& outDirection) const
    {
        if (!mPreviewCamera || !mPreviewCamera->HasComponent<XJCameraComponent>() 
                            || !mPreviewCamera->HasComponent<XJTransformComponent>())
            return false;

        // 获取鼠标在视口中的位置
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 relativeMousePos = ImVec2(mousePos.x - imageMin.x, mousePos.y - imageMin.y);

        // 将鼠标位置转换为NDC坐标
        const float ndcX = (relativeMousePos.x / imageSize.x) * 2.0f - 1.0f;
        //float ndcY = 1.0f - (relativeMousePos.y / imageSize.y) * 2.0f; // Y轴需要翻转
        const float ndcY = (relativeMousePos.y / imageSize.y) * 2.0f - 1.0f; // Y轴需要翻转

        // 获取摄像机的投影矩阵和视图矩阵
        auto& cameraComp = mPreviewCamera->GetComponent<XJCameraComponent>();
        auto& transformComp = mPreviewCamera->GetComponent<XJTransformComponent>();

        const glm::mat4 invProjection = glm::inverse(cameraComp.XJGetProjectionMatrix());
        const glm::mat4 invView = glm::inverse(cameraComp.XJGetViewMatrix());

        // 计算从屏幕空间到世界空间的射线
        const glm::vec4 rayClip(ndcX, ndcY, 0.0f, 1.0f);
        glm::vec4 rayEye = invProjection * rayClip;

        if (std::abs(rayEye.w) > 0.000001f)
            rayEye /= rayEye.w;

        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

        const glm::vec3 rayWorld = glm::normalize(glm::vec3(invView * rayEye));

        // 假设我们想将物体放置在距离摄像机一定距离的位置，例如10单位
        //float distanceFromCamera = 10.0f;
//
        //if(std::abs(rayWorld.y) > 0.001f) //避免与水平面平行的情况
        //{
        //    distanceFromCamera = -transformComp.position.y / rayWorld.y;//计算射线与水平面的交点距离
        //}
        //glm::vec3 dropPosition = transformComp.position + rayWorld * distanceFromCamera;

        outOrigin = transformComp.position;
        outDirection = rayWorld;

        return true;
    }

    void XJScenePreview::DrawUI()
    {
        mHovered = false;
        mFocused = false;
        // 处理拖放事件
        if(!mOpen)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));//去掉窗口内边距，让渲染图像填满整个窗口

        if(ImGui::Begin(mSettings.mViewportName, &mOpen))
        {
            mFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            ImVec2 avail = ImGui::GetContentRegionAvail();

            if(avail.x > 1.0f && avail.y > 1.0f)
            {
                Resize(static_cast<uint32_t>(avail.x), static_cast<uint32_t>(avail.y));

                if(IsViewportTextureReady())
                {
                    ImVec2 imageMin = ImGui::GetCursorScreenPos();//获取当前 ImGui 光标在屏幕上的位置，这个位置对应于我们即将绘制的图像的左上角

                    ImGui::Image(GetViewportTextureID(), avail);
                    mHovered = ImGui::IsItemHovered();
                    
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(XJ_ASSET_PAYLOAD_NAME))
                        {
                            if (payload->DataSize == sizeof(XJEditorAssetDragPayload))
                            {
                                const auto* assetPayload = static_cast<const XJEditorAssetDragPayload*>(payload->Data);
                                if (assetPayload && mAssetDropCallback)
                                {
                                    XJAssetDragPayload droppedPayload{};
                                    droppedPayload.Handle = assetPayload->Handle;
                                    droppedPayload.Type = assetPayload->Type;
                                
                                    glm::vec3 rayOrigin;
                                    glm::vec3 rayDirection;
                                    if (CalculateDropPositionFromViewportRay(imageMin, avail, rayOrigin, rayDirection))
                                    {
                                        droppedPayload.HasViewportRay = true;
                                        droppedPayload.RayOrigin = rayOrigin;
                                        droppedPayload.RayDirection = rayDirection;
                                    }
                                
                                    mAssetDropCallback(droppedPayload);
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
                else
                {
                    ImGui::TextUnformatted("Scene Preview Texture Invalid.");
                }
            }
            else
            {
                ImGui::TextUnformatted("Preview window too small.");
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
      
    }

     
}
