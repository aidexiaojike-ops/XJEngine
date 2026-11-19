#include "Render/System/XJSceneGizmoMaterialSystem.h"

#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJLightComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJScene.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/XJVulkanVertex.h"
#include "Render/Resource/XJMesh.h"
#include "Render/XJRenderTarget.h"
#include "Tools/XJCameraGizmoGeometry.h"
#include "Tools/XJLightGizmoGeometry.h"

#include <cstddef>
#include <vector>

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

        template <typename Positions, typename Indices>
        std::shared_ptr<XJMesh> CreateGizmoMesh(const Positions& positions, const Indices& lineIndices)
        {
            std::vector<XJVulkanVertex> vertices;
            vertices.reserve(positions.size());
            for (const glm::vec3& position : positions)
            {
                XJVulkanVertex vertex{};
                vertex.position = position;
                vertices.push_back(vertex);
            }
            const std::vector<uint32_t> indices(lineIndices.begin(), lineIndices.end());
            return std::make_shared<XJMesh>(vertices, indices);
        }

        std::shared_ptr<XJMesh> CreateLightGizmoMesh(const LightGizmoGeometry::LineGeometry& geometry)
        {
            return CreateGizmoMesh(geometry.Vertices, geometry.Indices);
        }
    }

    void XJSceneGizmoMaterialSystem::OnInit(XJVulkanRenderPass* renderPass)
    {
        XJVulkanDevice* device = XJGetDevice();
        if (!device || !renderPass)
            return;

        mDirectionalMesh = CreateLightGizmoMesh(LightGizmoGeometry::BuildDirectionalGeometry());
        mPointMesh = CreateLightGizmoMesh(LightGizmoGeometry::BuildPointGeometry());
        mSpotMesh = CreateLightGizmoMesh(LightGizmoGeometry::BuildSpotGeometry());
        mCameraMesh = CreateGizmoMesh(CameraGizmoGeometry::Vertices, CameraGizmoGeometry::Indices);

        const VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(GizmoPushConstants)
        };
        ShaderLayout shaderLayout;
        shaderLayout.pushConstantRanges = {pushConstantRange};

        mPipelineLayout = std::make_shared<XJVulkanPipelineLayout>(
            device,
            "Resource/Shader/EditorLightGizmo.vert",
            "Resource/Shader/EditorLightGizmo.frag",
            shaderLayout);

        std::vector<VkVertexInputBindingDescription> bindings = {{
            .binding = 0,
            .stride = sizeof(XJVulkanVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        }};
        std::vector<VkVertexInputAttributeDescription> attributes = {{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(XJVulkanVertex, position)
        }};

        mPipeline = std::make_shared<XJVulkanPipeline>(device, renderPass, mPipelineLayout.get());
        mPipeline->SetVertexInputState(bindings, attributes);
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
        mPipeline->SetDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
        mPipeline->SetMultisampleState(VK_SAMPLE_COUNT_1_BIT, VK_FALSE);
        PipelineDepthStencilState depthState;
        depthState.depthTestEnable = VK_TRUE;
        depthState.depthWriteEnable = VK_FALSE;
        depthState.depthCompareOp = VK_COMPARE_OP_LESS;
        mPipeline->SetDepthStencilState(depthState);
        mPipeline->Create();
    }

    void XJSceneGizmoMaterialSystem::OnRender(VkCommandBuffer cmdBuffer, XJRenderTarget* renderTarget)
    {
        XJScene* scene = XJGetScene();
        if (!scene || !renderTarget || !mPipeline || !mPipeline->IsValid() || !mPipelineLayout)
            return;

        XJVulkanFrameBuffer* frameBuffer = renderTarget->XJGetCurrentFrameBuffer();
        if (!frameBuffer)
            return;

        const auto& registry = scene->XJGetEcsRegistry();
        auto lights = registry.view<XJTransformComponent, XJLightComponent>();
        auto cameras = registry.view<XJCameraComponent>();
        if (lights.begin() == lights.end() && cameras.begin() == cameras.end())
            return;

        mPipeline->BindPipeline(cmdBuffer);

        VkViewport viewport{};
        viewport.width = static_cast<float>(frameBuffer->XJGetWidth());
        viewport.height = static_cast<float>(frameBuffer->XJGetHeight());
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = {frameBuffer->XJGetWidth(), frameBuffer->XJGetHeight()};
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        const glm::mat4 viewProjection = XJGetProjMat(renderTarget) * XJGetViewMat(renderTarget);
        const auto draw = [&](XJMesh* mesh, const glm::mat4& model, const glm::vec4& color)
        {
            if (!mesh || !mesh->IsValid())
                return;

            const GizmoPushConstants pushConstants{viewProjection * model, color};
            vkCmdPushConstants(
                cmdBuffer,
                mPipelineLayout->XJGetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(pushConstants),
                &pushConstants);
            if (mesh->Draw(cmdBuffer))
                renderTarget->RecordDrawCall();
        };

        lights.each([&](auto, const XJTransformComponent& transform, const XJLightComponent& light)
        {
            XJMesh* mesh = nullptr;

            switch (light.XJGetLightType())
            {
                case XJLightType::Directional:
                    mesh = mDirectionalMesh.get();
                    break;
                case XJLightType::Point:
                    mesh = mPointMesh.get();
                    break;
                case XJLightType::Spot:
                    mesh = mSpotMesh.get();
                    break;
            }

            glm::mat4 model;
            if (!LightGizmoGeometry::TryBuildModel(transform, light, model))
                return;

            const glm::vec4 color = light.XJGetEnable()
                ? glm::vec4(glm::clamp(light.XJGetColor(), glm::vec3(0.0f), glm::vec3(1.0f)), 1.0f)
                : glm::vec4(0.35f, 0.35f, 0.35f, 1.0f);

            draw(mesh, model, color);
        });

        cameras.each([&](const XJCameraComponent& camera)
        {
            const auto* entity = camera.XJGetOwner();
            if (!entity || entity->XJGetUUID() == renderTarget->XJGetCameraId())
                return;

            glm::mat4 model;
            if (!CameraGizmoGeometry::TryBuildModel(camera, model))
                return;
            const glm::vec4 color = camera.XJGetEnabled()
                ? glm::vec4(0.35f, 0.75f, 1.0f, 1.0f)
                : glm::vec4(0.35f, 0.35f, 0.35f, 1.0f);
            draw(mCameraMesh.get(), model, color);
        });
    }

    void XJSceneGizmoMaterialSystem::OnDestroy()
    {
        mPipeline.reset();
        mPipelineLayout.reset();
        mCameraMesh.reset();
        mSpotMesh.reset();
        mPointMesh.reset();
        mDirectionalMesh.reset();
    }
}
