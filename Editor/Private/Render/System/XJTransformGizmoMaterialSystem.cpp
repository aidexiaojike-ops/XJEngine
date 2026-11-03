#include "Render/System/XJTransformGizmoMaterialSystem.h"

#include "ECS/Component/XJCameraComponent.h"
#include "ECS/XJEntity.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/XJVulkanVertex.h"
#include "Render/Resource/XJMesh.h"
#include "Render/XJRenderTarget.h"
#include "UI/XJEditorUIState.h"

#include <cstddef>
#include <utility>

namespace XJ
{
    namespace
    {
        struct GizmoPushConstants
        {
            glm::mat4 Mvp{1.0f};
            glm::vec4 Color{1.0f};
        };

        static_assert(sizeof(GizmoPushConstants) == 80);
        static_assert(offsetof(GizmoPushConstants, Color) == 64);
    }

    XJTransformGizmoMaterialSystem::XJTransformGizmoMaterialSystem(const XJEditorUIState* state) : mState(state)
    {
    }

    void XJTransformGizmoMaterialSystem::OnInit(
        XJVulkanRenderPass* renderPass)
    {
        auto* device = XJGetDevice();

        if (!device || !renderPass)
            return;

        const auto geometry =
            BuildTransformGizmoGeometry();

        mParts.reserve(geometry.size());

        for (const auto& source : geometry)
        {
            std::vector<XJVulkanVertex> vertices;
            vertices.reserve(source.Positions.size());

            for (const auto& position : source.Positions)
            {
                XJVulkanVertex vertex{};
                vertex.position = position;

                vertices.push_back(vertex);
            }

            Part part;
            part.Handle = source.Handle;
            part.Mesh =
                std::make_shared<XJMesh>(vertices);

            mParts.push_back(std::move(part));
        }

        ShaderLayout shaderLayout;

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(GizmoPushConstants);

        shaderLayout.pushConstantRanges.push_back(
            pushRange);

        mPipelineLayout =
            std::make_shared<XJVulkanPipelineLayout>(
                device,
                "Resource/Shader/EditorTransformGizmo.vert",
                "Resource/Shader/EditorTransformGizmo.frag",
                shaderLayout);

        const std::vector<VkVertexInputBindingDescription>
            bindings = {{
                .binding = 0,
                .stride = sizeof(XJVulkanVertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }};

        const std::vector<VkVertexInputAttributeDescription>
            attributes = {{
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(XJVulkanVertex, position)
            }};

        mPipeline =
            std::make_shared<XJVulkanPipeline>(
                device,
                renderPass,
                mPipelineLayout.get());

        mPipeline->SetVertexInputState(
            bindings,
            attributes);

        mPipeline->SetInputAssemblyState(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        PipelineRasterizationState rasterization;

        // 本版使用无光照纯色。
        // 双面绘制避免几何朝向影响编辑器手柄可见性。
        rasterization.cullMode = VK_CULL_MODE_NONE;

        mPipeline->SetRasterizationState(
            rasterization);

        mPipeline->SetDynamicState({
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        });

        mPipeline->SetMultisampleState(
            VK_SAMPLE_COUNT_1_BIT,
            VK_FALSE);

        PipelineDepthStencilState depth;
        depth.depthTestEnable = VK_FALSE;
        depth.depthWriteEnable = VK_FALSE;

        mPipeline->SetDepthStencilState(depth);

        mPipeline->Create();
    }

    void XJTransformGizmoMaterialSystem::OnRender(
        XJVulkanCommandBuffer cmdBuffer,
        XJRenderTarget* renderTarget)
    {
        if (!mState ||
            !renderTarget ||
            !mPipeline ||
            !mPipeline->IsValid() ||
            !mPipelineLayout)
        {
            return;
        }

        const auto& gizmo = mState->TransformGizmo;

        if (!gizmo.Visible ||
            mState->WorkspaceReadOnly ||
            mState->RuntimeInspection ||
            !mState->TransformTools.CanChangeSpace ||
            gizmo.EntityId !=
                mState->Selection.SelectedEntity ||
            gizmo.Space != mState->TransformTools.Space)
        {
            return;
        }

        auto* cameraEntity =
            renderTarget->XJGetCamera();

        if (!cameraEntity ||
            !cameraEntity->HasComponent<XJCameraComponent>())
        {
            return;
        }

        auto& camera =
            cameraEntity->GetComponent<XJCameraComponent>();

        if (!camera.XJGetEnabled())
            return;

        auto* frameBuffer =
            renderTarget->XJGetCurrentFrameBuffer();

        if (!frameBuffer)
            return;

        const uint32_t width =
            frameBuffer->XJGetWidth();

        const uint32_t height =
            frameBuffer->XJGetHeight();

        const glm::mat4 view =
            XJGetViewMat(renderTarget);

        const glm::mat4 projection =
            XJGetProjMat(renderTarget);

        const auto frame =
            BuildTransformGizmoFrame(
                gizmo.WorldPosition,
                gizmo.Orientation,
                view,
                projection,
                width,
                height);

        if (!frame.Valid)
            return;

        mPipeline->BindPipeline(cmdBuffer);

        VkViewport viewport{};
        viewport.width = float(width);
        viewport.height = float(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(
            cmdBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = {width, height};

        vkCmdSetScissor(
            cmdBuffer, 0, 1, &scissor);

        GizmoPushConstants push;
        push.Mvp =
            projection * view * frame.Model;

        for (const auto& part : mParts)
        {
            if (!part.Mesh || !part.Mesh->IsValid())
                continue;

            push.Color =
                GetGizmoAxisColor(
                    GetGizmoAxis(part.Handle));

            vkCmdPushConstants(
                cmdBuffer,
                mPipelineLayout->XJGetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(push),
                &push);

            if (part.Mesh->Draw(cmdBuffer))
                renderTarget->RecordDrawCall();
        }
    }

    void XJTransformGizmoMaterialSystem::OnDestroy()
    {
        mPipeline.reset();
        mPipelineLayout.reset();
        mParts.clear();
    }
}