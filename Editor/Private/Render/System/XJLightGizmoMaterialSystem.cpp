#include "Render/System/XJLightGizmoMaterialSystem.h"

#include "ECS/Component/XJLightComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJScene.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/XJVulkanVertex.h"
#include "Render/Resource/XJMesh.h"
#include "Render/XJLightSceneUtils.h"
#include "Render/XJRenderTarget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <glm/gtc/constants.hpp>
#include <vector>

namespace XJ
{
    namespace
    {
        constexpr uint32_t kCircleSegments = 20;
        constexpr float kPointRadius = 0.3f;
        constexpr float kDirectionalLength = 0.8f;
        constexpr float kSpotLength = 0.8f;

        struct LightGizmoPushConstants
        {
            glm::mat4 Mvp{1.0f};
            glm::vec4 Color{1.0f};
        };

        static_assert(sizeof(LightGizmoPushConstants) == 80);

        uint32_t AddVertex(std::vector<XJVulkanVertex>& vertices, const glm::vec3& position)
        {
            XJVulkanVertex vertex{};
            vertex.position = position;
            vertices.push_back(vertex);
            return static_cast<uint32_t>(vertices.size() - 1);
        }

        void AddLine(
            std::vector<XJVulkanVertex>& vertices,
            std::vector<uint32_t>& indices,
            const glm::vec3& start,
            const glm::vec3& end)
        {
            indices.push_back(AddVertex(vertices, start));
            indices.push_back(AddVertex(vertices, end));
        }

        void AddCircle(
            std::vector<XJVulkanVertex>& vertices,
            std::vector<uint32_t>& indices,
            int fixedAxis,
            float fixedValue,
            float radius)
        {
            for (uint32_t segment = 0; segment < kCircleSegments; ++segment)
            {
                const float a0 = glm::two_pi<float>() * static_cast<float>(segment) / static_cast<float>(kCircleSegments);
                const float a1 = glm::two_pi<float>() * static_cast<float>(segment + 1) / static_cast<float>(kCircleSegments);
                glm::vec3 p0(0.0f);
                glm::vec3 p1(0.0f);
                p0[fixedAxis] = fixedValue;
                p1[fixedAxis] = fixedValue;

                const int axisA = (fixedAxis + 1) % 3;
                const int axisB = (fixedAxis + 2) % 3;
                p0[axisA] = std::cos(a0) * radius;
                p0[axisB] = std::sin(a0) * radius;
                p1[axisA] = std::cos(a1) * radius;
                p1[axisB] = std::sin(a1) * radius;
                AddLine(vertices, indices, p0, p1);
            }
        }

        std::shared_ptr<XJMesh> CreatePointMesh()
        {
            std::vector<XJVulkanVertex> vertices;
            std::vector<uint32_t> indices;
            vertices.reserve(kCircleSegments * 6);
            indices.reserve(kCircleSegments * 6);
            AddCircle(vertices, indices, 0, 0.0f, kPointRadius);
            AddCircle(vertices, indices, 1, 0.0f, kPointRadius);
            AddCircle(vertices, indices, 2, 0.0f, kPointRadius);
            return std::make_shared<XJMesh>(vertices, indices);
        }

        std::shared_ptr<XJMesh> CreateDirectionalMesh()
        {
            std::vector<XJVulkanVertex> vertices;
            std::vector<uint32_t> indices;
            AddLine(vertices, indices, glm::vec3(0.0f), glm::vec3(kDirectionalLength, 0.0f, 0.0f));

            constexpr float headX = 0.55f;
            constexpr float headRadius = 0.16f;
            for (uint32_t segment = 0; segment < 8; ++segment)
            {
                const float angle = glm::two_pi<float>() * static_cast<float>(segment) / 8.0f;
                AddLine(
                    vertices,
                    indices,
                    glm::vec3(kDirectionalLength, 0.0f, 0.0f),
                    glm::vec3(headX, std::cos(angle) * headRadius, std::sin(angle) * headRadius));
            }
            AddCircle(vertices, indices, 0, headX, headRadius);
            return std::make_shared<XJMesh>(vertices, indices);
        }

        std::shared_ptr<XJMesh> CreateSpotMesh()
        {
            std::vector<XJVulkanVertex> vertices;
            std::vector<uint32_t> indices;
            AddLine(vertices, indices, glm::vec3(0.0f), glm::vec3(kSpotLength, 0.0f, 0.0f));
            AddCircle(vertices, indices, 0, kSpotLength, 1.0f);

            for (uint32_t segment = 0; segment < 8; ++segment)
            {
                const float angle = glm::two_pi<float>() * static_cast<float>(segment) / 8.0f;
                AddLine(
                    vertices,
                    indices,
                    glm::vec3(0.0f),
                    glm::vec3(kSpotLength, std::cos(angle), std::sin(angle)));
            }
            return std::make_shared<XJMesh>(vertices, indices);
        }

        glm::mat4 BuildDirectionRotation(const glm::vec3& direction)
        {
            const glm::vec3 xAxis = glm::normalize(direction);
            const glm::vec3 referenceUp = std::abs(glm::dot(xAxis, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f
                ? glm::vec3(0.0f, 0.0f, 1.0f)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 yAxis = glm::normalize(referenceUp - xAxis * glm::dot(referenceUp, xAxis));
            const glm::vec3 zAxis = glm::normalize(glm::cross(xAxis, yAxis));

            glm::mat4 result(1.0f);
            result[0] = glm::vec4(xAxis, 0.0f);
            result[1] = glm::vec4(yAxis, 0.0f);
            result[2] = glm::vec4(zAxis, 0.0f);
            return result;
        }
    }

    void XJLightGizmoMaterialSystem::OnInit(XJVulkanRenderPass* renderPass)
    {
        XJVulkanDevice* device = XJGetDevice();
        if (!device || !renderPass)
            return;

        mDirectionalMesh = CreateDirectionalMesh();
        mPointMesh = CreatePointMesh();
        mSpotMesh = CreateSpotMesh();

        const VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(LightGizmoPushConstants)
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

    void XJLightGizmoMaterialSystem::OnRender(VkCommandBuffer cmdBuffer, XJRenderTarget* renderTarget)
    {
        XJScene* scene = XJGetScene();
        if (!scene || !renderTarget || !mPipeline || !mPipeline->IsValid() || !mPipelineLayout)
            return;

        XJVulkanFrameBuffer* frameBuffer = renderTarget->XJGetCurrentFrameBuffer();
        if (!frameBuffer)
            return;

        const auto& registry = scene->XJGetEcsRegistry();
        auto view = registry.view<XJTransformComponent, XJLightComponent>();
        if (view.begin() == view.end())
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
        view.each([&](auto, const XJTransformComponent& transform, const XJLightComponent& light)
        {
            XJMesh* mesh = nullptr;
            glm::vec3 scale(1.0f);

            switch (light.XJGetLightType())
            {
                case XJLightType::Directional:
                    mesh = mDirectionalMesh.get();
                    break;
                case XJLightType::Point:
                    mesh = mPointMesh.get();
                    break;
                case XJLightType::Spot:
                {
                    mesh = mSpotMesh.get();
                    const float coneRadius = std::min(
                        std::tan(glm::radians(light.XJGetOuterAngleDegrees())) * kSpotLength,
                        1.0f);
                    scale.y = coneRadius;
                    scale.z = coneRadius;
                    break;
                }
            }

            if (!mesh || !mesh->IsValid())
                return;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position);
            if (light.XJGetLightType() != XJLightType::Point)
                model *= BuildDirectionRotation(XJLightSceneUtils::BuildDirection(transform));
            model = glm::scale(model, scale);

            LightGizmoPushConstants pushConstants;
            pushConstants.Mvp = viewProjection * model;
            pushConstants.Color = light.XJGetEnable()
                ? glm::vec4(glm::clamp(light.XJGetColor(), glm::vec3(0.0f), glm::vec3(1.0f)), 1.0f)
                : glm::vec4(0.35f, 0.35f, 0.35f, 1.0f);

            vkCmdPushConstants(
                cmdBuffer,
                mPipelineLayout->XJGetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(pushConstants),
                &pushConstants);
            mesh->Draw(cmdBuffer);
        });
    }

    void XJLightGizmoMaterialSystem::OnDestroy()
    {
        mPipeline.reset();
        mPipelineLayout.reset();
        mSpotMesh.reset();
        mPointMesh.reset();
        mDirectionalMesh.reset();
    }
}
