#include "Render/System/XJSelectionOutlineMaterialSystem.h"

#include "ECS/Component/Material/XJSurfaceMaterialComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/XJVulkanVertex.h"
#include "Render/Resource/XJMesh.h"
#include "Render/XJRenderTarget.h"
#include "Tools/XJOutlineNormals.h"
#include "UI/XJEditorUIState.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

namespace XJ
{
    namespace
    {
        constexpr float kOutlinePixels = 2.0f;//描边宽度橡树

        constexpr glm::vec4 kOutlineColor{
            1.0f, 0.55f, 0.12f, 1.0f
        };//描边颜色

        // 与 Shader 中的布局完全一致。
        //
        // Color.w 保存 NDC 空间厚度。
        struct alignas(16) OutlinePushConstants// 常量与 Push Constants 布局
        {
            glm::mat4 Mvp{1.0f};

            // normalToClip 的三列。
            glm::vec4 NormalColumn0{0.0f};
            glm::vec4 NormalColumn1{0.0f};
            glm::vec4 NormalColumn2{0.0f};

            glm::vec4 Color{1.0f};
        };

        static_assert(sizeof(OutlinePushConstants) == 128);
        static_assert(offsetof(OutlinePushConstants, NormalColumn0) == 64);
        static_assert(offsetof(OutlinePushConstants, NormalColumn1) == 80);
        static_assert(offsetof(OutlinePushConstants, NormalColumn2) == 96);
        static_assert(offsetof(OutlinePushConstants, Color) == 112);
    }

    XJSelectionOutlineMaterialSystem::XJSelectionOutlineMaterialSystem(
        XJEditorUIState* state)
        : mState(state)
    {
    }

    void XJSelectionOutlineMaterialSystem::OnInit(
        XJVulkanRenderPass* renderPass)
    {
        auto* device = XJGetDevice();

        if (!device || !renderPass)
            return;

        ShaderLayout shaderLayout;
        //管线布局
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(OutlinePushConstants);

        shaderLayout.pushConstantRanges.push_back(
            pushRange);
        //绑定着色器
        mPipelineLayout =
            std::make_shared<XJVulkanPipelineLayout>(
                device,
                "Resource/Shader/EditorSelectionOutline.vert",
                "Resource/Shader/EditorSelectionOutline.frag",
                shaderLayout);

        const std::vector<VkVertexInputBindingDescription>
            bindings = {{
                .binding = 0,
                .stride = sizeof(XJVulkanVertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }};
        //两个顶点属性
        const std::vector<VkVertexInputAttributeDescription>
            attributes = {// 位置
                {
                    .location = 0,
                    .binding = 0,
                    .format = VK_FORMAT_R32G32B32_SFLOAT,
                    .offset = offsetof(
                        XJVulkanVertex,
                        position)
                },
                { // 法线
                    .location = 2,
                    .binding = 0,
                    .format = VK_FORMAT_R32G32B32_SFLOAT,
                    .offset = offsetof(
                        XJVulkanVertex,
                        normal)
                }
            };

        const auto createPipeline =
            [&](VkCullModeFlags cullMode)
        {
            auto pipeline =
                std::make_shared<XJVulkanPipeline>(
                    device,
                    renderPass,
                    mPipelineLayout.get());

            pipeline->SetVertexInputState(
                bindings,
                attributes);

            pipeline->SetInputAssemblyState(
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

            PipelineRasterizationState rasterization;

            // 反向壳：正面朝向模型的壳不画，
            // 只保留扩张后的背面轮廓。
            rasterization.cullMode = cullMode;
            rasterization.frontFace =
                VK_FRONT_FACE_COUNTER_CLOCKWISE;

            pipeline->SetRasterizationState(
                rasterization);

            pipeline->SetDynamicState({
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            });

            pipeline->SetMultisampleState(
                VK_SAMPLE_COUNT_1_BIT,
                VK_FALSE);

            PipelineDepthStencilState depth;

            // 读取场景深度，避免轮廓盖住前景物体。
            depth.depthTestEnable = VK_TRUE;
            depth.depthWriteEnable = VK_FALSE;
            depth.depthCompareOp = VK_COMPARE_OP_LESS;

            pipeline->SetDepthStencilState(depth);

            pipeline->Create();
            return pipeline;
        };

        mPipelineCullFront = createPipeline(VK_CULL_MODE_FRONT_BIT); // 剔除正面
        mPipelineCullBack = createPipeline(VK_CULL_MODE_BACK_BIT); // 剔除背面
    }

    std::shared_ptr<XJMesh> XJSelectionOutlineMaterialSystem::GetOutlineMesh(const std::shared_ptr<XJMesh>& source)
    {
        if (!source || !source->IsValid())
            return nullptr;

        const auto& positions =
            source->GetCpuPositions();

        const auto& indices =
            source->GetCpuIndices();

        // 当前 Surface 渲染依赖 submesh 索引范围。
        // 没有索引或没有 submesh 的 Mesh 不生成轮廓。
        if (positions.empty() || indices.empty() || source->GetSubmeshCount() == 0)
            return nullptr;

        const auto found = mCache.find(source.get());

        if (found != mCache.end())//缓存查找 + ABA 防护
        {
            const auto existing =
                found->second.Source.lock();

            if (existing && existing.get() == source.get())
            {
                return found->second.Outline;// 命中，源还活着且地址一致
            }

            mCache.erase(found);// 源已销毁，或地址被复用，删掉重建
        }

        const auto normals =
            BuildSmoothOutlineNormals(
                positions,
                indices);

        if (normals.size() != positions.size())
            return nullptr;

        std::vector<XJVulkanVertex> vertices;
        vertices.reserve(positions.size());

        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            XJVulkanVertex vertex{};
            vertex.position = positions[i];
            vertex.normal = normals[i]; // ← 用平滑轮廓法线替换原法线
            vertices.push_back(vertex);
        }

        std::shared_ptr<XJMesh> outline;

        try
        {
            outline = std::make_shared<XJMesh>(
                vertices,
                indices,
                source->GetSubmeshes());
        }
        catch (const std::exception& error)
        {
            spdlog::warn(
                "Selection outline mesh creation "
                "failed: {}",
                error.what());

            return nullptr;
        }

        if (!outline || !outline->IsValid())
            return nullptr;

        OutlineMeshEntry entry;
        entry.Source = source;
        entry.Outline = outline;

        mCache[source.get()] = std::move(entry);

        if (mCache.size() > 256)
            PruneCache();

        return outline;
    }

    void XJSelectionOutlineMaterialSystem::PruneCache()
    {
        for (auto it = mCache.begin();
             it != mCache.end();)
        {
            if (it->second.Source.expired())
                it = mCache.erase(it);
            else
                ++it;
        }
    }

    void XJSelectionOutlineMaterialSystem::OnRender(VkCommandBuffer cmdBuffer, XJRenderTarget* renderTarget)
    {
        //环境守卫
        if (!mState || !renderTarget || !mPipelineLayout || !mPipelineCullFront || !mPipelineCullBack)
            return;
        // 编辑态专用。Play、Pause 和运行时检查都不显示。
        if (mState->WorkspaceReadOnly || mState->RuntimeInspection)
            return;
        // 选中实体解析
        const XJEditorEntityId selected = mState->Selection.SelectedEntity;
        if (selected == XJ_INVALID_EDITOR_ENTITY_ID)
            return;

        XJScene* scene = XJGetScene();

        if (!scene)
            return;

        XJEntity* entity = scene->FindEntityByUUID(XJUUID{selected});

        if (!entity ||
            !entity->IsValid() ||
            !entity->HasComponent<XJTransformComponent>() ||
            !entity->HasComponent<XJSurfaceMaterialComponent>())
        {
            return;
        }

        auto* frameBuffer =
            renderTarget->XJGetCurrentFrameBuffer();

        if (!frameBuffer)
            return;

        const uint32_t width =
            frameBuffer->XJGetWidth();

        const uint32_t height =
            frameBuffer->XJGetHeight();

        if (width == 0 || height == 0)
            return;

         //模型矩阵 + 行列式守卫
        glm::mat4 model;
        if (entity->GetComponent<XJTransformComponent>().ReadWorldMatrix(model) != XJTransformStatus::Success)
        {
            return;
        }

        const glm::mat3 model3(model);
        const float determinant = glm::determinant(model3);

        if (!std::isfinite(determinant) ||
            std::abs(determinant) < 1.0e-12f)
        {
            return;
        }

        const glm::mat4 view = XJGetViewMat(renderTarget);

        const glm::mat4 projection =  XJGetProjMat(renderTarget);

        // 法线从模型空间直接转到裁剪空间方向。
        // 负缩放通过逆转置保留正确的向外方向。
        const glm::mat3 normalMatrix = glm::inverseTranspose(model3);//法线 → 裁剪空间

        const glm::mat3 normalToClip =
            glm::mat3(projection) *
            glm::mat3(view) *
            normalMatrix;

        for (int column = 0; column < 3; ++column)
        {
            for (int row = 0; row < 3; ++row)
            {
                if (!std::isfinite(
                        normalToClip[column][row]))
                {
                    return;
                }
            }
        }

        const auto& pipeline =
            determinant > 0.0f
                ? mPipelineCullFront
                : mPipelineCullBack;

        if (!pipeline || !pipeline->IsValid())
            return;

        pipeline->BindPipeline(cmdBuffer);

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

        OutlinePushConstants push;
        push.Mvp = projection * view * model;

        push.NormalColumn0 =  glm::vec4(normalToClip[0], 0.0f);

        push.NormalColumn1 = glm::vec4(normalToClip[1], 0.0f);

        push.NormalColumn2 = glm::vec4(normalToClip[2], 0.0f);

        // 2 像素对应的 NDC 偏移。
        push.Color = glm::vec4(
            glm::vec3(kOutlineColor),
            2.0f * kOutlinePixels / float(height));

        const auto& slots =
            entity->GetComponent<XJSurfaceMaterialComponent>()
                .XJGetSlots();

        std::vector<std::pair<const XJMesh*, uint32_t>>
            drawn;

        drawn.reserve(slots.size());
            //有限性检查
        for (const auto& slot : slots)
        {
            const auto outline =
                GetOutlineMesh(slot.Mesh);

            if (!outline ||
                slot.SubmeshIndex >=
                    outline->GetSubmeshCount())
            {
                continue;
            }

            const bool duplicate =
                std::any_of(
                    drawn.begin(),
                    drawn.end(),
                    [&](const auto& item)
                {
                    return
                        item.first == slot.Mesh.get() &&
                        item.second == slot.SubmeshIndex;
                });

            if (duplicate)
                continue;

            vkCmdPushConstants(
                cmdBuffer,
                mPipelineLayout->XJGetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(push),
                &push);

            if (outline->DrawSubmesh(
                    cmdBuffer,
                    slot.SubmeshIndex))
            {
                renderTarget->RecordDrawCall();
            }

            drawn.emplace_back(
                slot.Mesh.get(),
                slot.SubmeshIndex);
        }
    }

    void XJSelectionOutlineMaterialSystem::OnDestroy()
    {
        mCache.clear();
        mPipelineCullBack.reset();
        mPipelineCullFront.reset();
        mPipelineLayout.reset();
    }

}
