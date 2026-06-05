#include "UI/Viewports/XJScenePreview.h"
// #include "imgui_impl_vulkan.h"
#include "Render/XJRenderTarget.h"
#include "ECS/XJEntity.h"
#include "Graphic/XJVulkanRenderPass.h"   
#include "Graphic/XJVulkanDevice.h"      
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include <glm/gtc/matrix_inverse.hpp> 

namespace XJ
{
    bool XJScenePreview::Render(VkCommandBuffer cmd)
    {
        //// ★ 先设摄像机
        if (mPreviewCamera)
            mRenderTarget->XJSetCamera(mPreviewCamera);
        if (!BeginViewportRender(cmd))   // BeginRenderTarget
            return false;


        mRenderTarget->RenderMaterialSystem(cmd);
        EndViewportRender(cmd);          // EndRenderTarget + RecreateDescriptor
        return true;
    }

    void XJScenePreview::CreateRenderPass(VulkanPhysicalDevices* physicalDevices)
    {
        VkFormat colorFormat = mDevice->XJGetSettings().surfaceFormat;
        VkFormat depthFormat = mDevice->XJGetSettings().depthFormat;

        Attachment colorAttachment{};
        colorAttachment.format = colorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        colorAttachment.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        
        Attachment depthAttachment{};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        std::vector<Attachment> attachments =
        {
            colorAttachment,
            depthAttachment
        };

        std::vector<RenderSubPass> subpasses =
        {
            {
                .colorAttachments = { 0 }, //颜色附件索引
                .depthStencilAttachments = { 1 },//深度附件索引
                .resolveAttachments = {},   // 解析附件由渲染通道自动添加
                .sampleCount = VK_SAMPLE_COUNT_1_BIT//采样数
            }
        };

        mRenderPass = std::make_shared<XJVulkanRenderPass>(
            mDevice,
            physicalDevices,
            attachments,
            subpasses
        );
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
        float ndcX = (relativeMousePos.x / imageSize.x) * 2.0f - 1.0f;
        //float ndcY = 1.0f - (relativeMousePos.y / imageSize.y) * 2.0f; // Y轴需要翻转
        float ndcY = (relativeMousePos.y / imageSize.y) * 2.0f - 1.0f; // Y轴需要翻转

        // 获取摄像机的投影矩阵和视图矩阵
        auto& cameraComp = mPreviewCamera->GetComponent<XJCameraComponent>();
        auto& transformComp = mPreviewCamera->GetComponent<XJTransformComponent>();


        glm::mat4 projectionMatrix = glm::inverse(cameraComp.XJGetProjectionMatrix());
        glm::mat4 viewMatrix = glm::inverse(cameraComp.XJGetViewMatrix());

        // 计算从屏幕空间到世界空间的射线
        glm::vec4 rayClip = glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 rayEye = projectionMatrix* rayClip;
        rayEye.z = 1.0f; rayEye.w = 0.0f;

        glm::vec3 rayWorld = glm::normalize(glm::vec3(viewMatrix * rayEye));

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

                if(mPendingResize)
                    mNeedDescriptorUpdate = true;
                
                if(!mPendingResize && mDescriptorSet != VK_NULL_HANDLE)
                {
                    ImVec2 imageMin = ImGui::GetCursorScreenPos();//获取当前 ImGui 光标在屏幕上的位置，这个位置对应于我们即将绘制的图像的左上角

                    ImGui::Image((ImTextureID)mDescriptorSet, avail);
                    mHovered = ImGui::IsItemHovered();
                    
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(XJ_ASSET_PAYLOAD_NAME))
                        {
                            const auto* assetPayload = static_cast<const XJAssetDragPayload*>(payload->Data);
                            if (assetPayload && mAssetDropCallback)
                            {
                                XJAssetDragPayload droppedPayload = *assetPayload;

                                // 计算拖放位置
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