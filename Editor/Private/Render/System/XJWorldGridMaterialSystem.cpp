#include "Render/System/XJWorldGridMaterialSystem.h"

#include "ECS/Component/XJCameraComponent.h"
#include "ECS/XJEntity.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Render/XJRenderTarget.h"
#include "Tools/XJEditorWorldGridLayout.h"

#include <cstddef>
#include <cstdint>

namespace XJ
{
    namespace
    {
        // 与 Shader 中的布局保持完全一致。
        //
        // Data.x:
        //   0 = 网格
        //   1 = X 轴文字及刻线
        //   2 = Z 轴文字及刻线
        //   3 = 格距说明
        //
        // Data.y:
        //   网格模式：HalfLineCount
        //   文字模式：字符数
        //
        // Data.z / Data.w:
        //   每字符占 4 bit，最多 16 个字符。
        struct alignas(16) WorldGridPushConstants//推送常量结构体
        {
            glm::mat4 ViewProjection{1.0f};

            glm::vec4 Anchor{0.0f};
            glm::vec4 Right{0.0f};
            glm::vec4 Up{0.0f};

            glm::uvec4 Data{0u};
        };

        static_assert(sizeof(WorldGridPushConstants) == 128);
        static_assert(offsetof(WorldGridPushConstants, Anchor) == 64);
        static_assert(offsetof(WorldGridPushConstants, Right) == 80);
        static_assert(offsetof(WorldGridPushConstants, Up) == 96);
        static_assert(offsetof(WorldGridPushConstants, Data) == 112);

        uint32_t EncodeCharacter(char value)//编码字符
        {
            if (value >= '0' && value <= '9')
                return uint32_t(value - '0');

            switch (value)
            {
                case '-': return 10u;
                case '.': return 11u;
                case 'm': return 12u;
                case '=': return 13u;
                case 'G': return 14u;
                default:  return 15u; // 空白
            }
        }

        glm::uvec2 PackText(const std::string& text)//打包文本
        {
            glm::uvec2 packed(0u);

            for (std::size_t i = 0;
                 i < text.size() && i < 16;
                 ++i)
            {
                const uint32_t code =
                    EncodeCharacter(text[i]);

                const unsigned int word =
                    unsigned(i / 8);

                const unsigned int shift =
                    unsigned(i % 8) * 4u;

                packed[word] |= code << shift;
            }

            return packed;
        }
    }

    void XJWorldGridMaterialSystem::OnInit(XJVulkanRenderPass* renderPass)//初始化
    {
        auto* device = XJGetDevice();

        ShaderLayout shaderLayout;
        // 这里的 push constant range 必须与 Shader 中的布局完全一致。
        VkPushConstantRange range{};
        range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        range.offset = 0;
        range.size = sizeof(WorldGridPushConstants);

        shaderLayout.pushConstantRanges.push_back(range);

        mPipelineLayout =
            std::make_shared<XJVulkanPipelineLayout>(
                device,
                "Resource/Shader/EditorWorldGrid.vert",
                "Resource/Shader/EditorWorldGrid.frag",
                shaderLayout);

        mPipeline =
            std::make_shared<XJVulkanPipeline>(
                device,
                renderPass,
                mPipelineLayout.get());

        // 顶点由 gl_VertexIndex 程序化生成。
        mPipeline->SetVertexInputState({}, {});
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
        mPipeline->SetDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
        mPipeline->SetMultisampleState(VK_SAMPLE_COUNT_1_BIT, VK_FALSE);

        PipelineDepthStencilState depth;
        depth.depthTestEnable = VK_TRUE;
        depth.depthWriteEnable = VK_FALSE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        mPipeline->SetDepthStencilState(depth);
        mPipeline->EnableAlphaBlend();

        mPipeline->Create();
    }

    void XJWorldGridMaterialSystem::OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget)//渲染
    {
        if (!renderTarget || !mPipeline || !mPipeline->IsValid() || !mPipelineLayout)
        {
            return;
        }

        auto* cameraEntity = renderTarget->XJGetCamera();

        if (!cameraEntity || !cameraEntity->HasComponent<XJCameraComponent>())
        {
            return;
        }

        auto& camera = cameraEntity->GetComponent<XJCameraComponent>();

        if (!camera.XJGetEnabled())
            return;

        // 不能用旧视图或单位矩阵掩盖相机 Transform 缺失、世界姿态求值失败。
        glm::mat4 view;
        if (!camera.TryGetViewMatrix(view))
            return;

        auto* frameBuffer = renderTarget->XJGetCurrentFrameBuffer();

        if (!frameBuffer)
            return;

        const uint32_t width = frameBuffer->XJGetWidth();
        const uint32_t height = frameBuffer->XJGetHeight();

        const glm::mat4 projection =
            camera.XJGetProjectionMatrix();

        const auto layout =
            XJEditorWorldGridLayoutBuilder::Build(
                view,
                projection,
                width,
                height);

        if (!layout.Valid)
            return;

        const glm::mat4 inverseView = glm::inverse(view);

        // 文字朝向相机，但仍然是真正的世界空间几何。
        const glm::vec3 cameraRight = glm::normalize(glm::vec3(inverseView[0]));
        const glm::vec3 cameraUp = glm::normalize(glm::vec3(inverseView[1]));

        mPipeline->BindPipeline(cmdBuffer);
        // 这里的 viewport 必须与 Shader 中的布局完全一致。
        VkViewport viewport{};
        viewport.width = float(width);
        viewport.height = float(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        // 这里的 viewport 必须与 Shader 中的布局完全一致。
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        VkRect2D scissor{};
        scissor.extent = {width, height};
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        const auto draw =
            [&](const WorldGridPushConstants& push,
                uint32_t vertexCount)
        {
            vkCmdPushConstants(cmdBuffer, mPipelineLayout->XJGetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);//推送常量

            vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
            renderTarget->RecordDrawCall();
        };

        WorldGridPushConstants push;
        push.ViewProjection = projection * view;

        // 网格一次 Draw。
        push.Anchor = glm::vec4(layout.Center, layout.Step);

        push.Data = {0u, uint32_t(layout.HalfLineCount), 0u, 0u};

        // 两个方向，每个方向 2N+1 条线，
        // 每条线两个顶点。
        const uint32_t gridVertexCount = uint32_t(2 * (2 * layout.HalfLineCount + 1) * 2);
        draw(push, gridVertexCount);

        // 每个标签一次 Draw。
        // 前两个顶点绘制短刻线，后面绘制线字。
        for (const auto& label : layout.Labels)
        {
            uint32_t mode = 3u;

            if (label.Kind == XJEditorGridLabelKind::XAxis)
                mode = 1u;
            else if (label.Kind == XJEditorGridLabelKind::ZAxis)
                mode = 2u;

            const glm::uvec2 packed =
                PackText(label.Text);

            push.Anchor = glm::vec4(label.Position, label.TickHalfLength);
            push.Right =glm::vec4(cameraRight * label.Height, 0.0f);
            push.Up = glm::vec4(cameraUp * label.Height, 0.0f);
            push.Data = {mode, uint32_t(label.Text.size()), packed.x, packed.y};

            draw(push, 2u + uint32_t(label.Text.size()) * 32u);
        }
    }

    void XJWorldGridMaterialSystem::OnDestroy()
    {
        mPipeline.reset();
        mPipelineLayout.reset();
    }
}
