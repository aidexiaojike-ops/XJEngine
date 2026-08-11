#include "Render/System/XJUnlitMaterialSystem.h"

#include "Graphic/XJVulkanDescriptorSet.h"
#include "Graphic/XJVulkanFrameBuffer.h"
#include "Graphic/XJVulkanPipeline.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/VulkanCommon.h"
#include "Render/Material/XJMaterialRuntimeUploader.h"
#include "Render/Material/XJUnlitMaterialRenderItemBuilder.h"
#include "Render/Resource/XJMaterial.h"
#include "Render/Resource/XJMaterialFactory.h"
#include "Render/Resource/XJMesh.h"
#include "Render/XJRenderTarget.h"

#include <glm/gtc/matrix_inverse.hpp>
#include "Edit\FileUtil.h"

namespace XJ
{ 
    namespace
    {
        constexpr uint32_t NUM_MATERIAL_BATCH = 16;
    }

    void XJUnlitMaterialSystem::OnInit(XJVulkanRenderPass *renderPass) 
    {//添加内容查看shader Uniform  UBO
       
        if (!InitializeMaterialRuntime(renderPass, "Resource/Shader/Unlit.xjshader"))
        {
            spdlog::error("Unlit material system failed to initialize pipeline runtime cache.");
            return;
        }

        XJMaterialPipelineRuntime* defaultRuntime = GetDefaultMaterialRuntime();
        if (!defaultRuntime || !defaultRuntime->IsValid())
        {
            spdlog::error("Unlit material system has no valid default pipeline runtime.");
            return;
        }

        //重新创建材质
        if (!ReCreateMaterialDescPool(*defaultRuntime, NUM_MATERIAL_BATCH))
        {
            spdlog::error("Unlit material system failed to create material descriptor pool.");
            return;
        }
       
    }
    void XJUnlitMaterialSystem::MarkMaterialParamsDirtyForAllFrameSlots(XJMaterialPipelineRuntime& runtime, uint32_t materialIndex)
    {
        std::vector<bool>& flags = mParamUploadedByRuntime[&runtime];
        const uint32_t flagCount = runtime.LastDescriptorSetCount*RENDERER_NUM_BUFFER;

        if (flags.size() < flagCount)
            flags.resize(flagCount, false);
        
        if(materialIndex >= runtime.LastDescriptorSetCount)
        {
            spdlog::error("MarkMaterialParamsDirtyForAllFrameSlots: materialIndex {} exceeds runtime.LastDescriptorSetCount {}",
                materialIndex, runtime.LastDescriptorSetCount);
            return;
        }

        for (uint32_t frameSlot = 0; frameSlot < RENDERER_NUM_BUFFER; ++frameSlot)
            flags[frameSlot * runtime.LastDescriptorSetCount + materialIndex] = false;
    }

    void XJUnlitMaterialSystem::MarkMaterialResourcesDirtyForAllFrameSlots(XJMaterialPipelineRuntime& runtime, uint32_t materialIndex)
    {
        std::vector<bool>& flags = mResourceUploadedByRuntime[&runtime];
        const uint32_t flagCount = runtime.LastDescriptorSetCount * RENDERER_NUM_BUFFER;

        if (flags.size() < flagCount)
            flags.resize(flagCount, false);

        if (materialIndex >= runtime.LastDescriptorSetCount)
            return;

        for (uint32_t frameSlot = 0; frameSlot < RENDERER_NUM_BUFFER; ++frameSlot)
            flags[frameSlot * runtime.LastDescriptorSetCount + materialIndex] = false;
    }

    bool XJUnlitMaterialSystem::HasPendingMaterialParamUpdates(XJMaterialPipelineRuntime& runtime, uint32_t materialIndex) const//
    {
        auto it = mParamUploadedByRuntime.find(&runtime);
        if (it == mParamUploadedByRuntime.end())
            return false;

        if (materialIndex >= runtime.LastDescriptorSetCount)
            return false;

        const std::vector<bool>& flags = it->second;
        for (uint32_t frameSlot = 0; frameSlot < RENDERER_NUM_BUFFER; ++frameSlot)
        {
            const uint32_t descriptorIndex = frameSlot * runtime.LastDescriptorSetCount + materialIndex;
            if (descriptorIndex < flags.size() && !flags[descriptorIndex])
                return true;
        }

        return false;
    }

    bool XJUnlitMaterialSystem::HasPendingMaterialResourceUpdates(XJMaterialPipelineRuntime& runtime, uint32_t materialIndex) const
    {
        auto it = mResourceUploadedByRuntime.find(&runtime);
        if (it == mResourceUploadedByRuntime.end())
            return false;

        if (materialIndex >= runtime.LastDescriptorSetCount)
            return false;

        const std::vector<bool>& flags = it->second;
        for (uint32_t frameSlot = 0; frameSlot < RENDERER_NUM_BUFFER; ++frameSlot)
        {
            const uint32_t descriptorIndex = frameSlot * runtime.LastDescriptorSetCount + materialIndex;
            if (descriptorIndex < flags.size() && !flags[descriptorIndex])
                return true;
        }

        return false;
    }

    void XJUnlitMaterialSystem::OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) 
    {
        XJScene *scene = XJGetScene();

        if(!scene){return;}//如果场景不存在，直接返回

        XJMaterialPipelineRuntime* defaultRuntime = GetDefaultMaterialRuntime();
        if (!defaultRuntime  || !defaultRuntime->IsValid())
        {
            spdlog::error("Unlit material system has no valid default pipeline runtime.");
            return;
        }

        XJUnlitMaterialRenderItemBuilder::Build(*scene, mRenderItems);
        if (mRenderItems.empty())
            return;// 视图确实为空
      
         //bind pipeline
        //runtime->Pipeline->BindPipeline(cmdBuffer);//绑定管线
        //vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->XJGetPipelineLayout(), 0, 1,  mDescriptorSets.data(), 0, nullptr);
        XJVulkanFrameBuffer* frameBuffer = renderTarget ? renderTarget->XJGetCurrentFrameBuffer() : nullptr;
        if (!frameBuffer) 
        {
            spdlog::error("FrameBuffer is null, skipping render");
            return;
        }
        //设置视口和裁剪矩形
        VkViewport kViewport{};
        kViewport.x = 0.0f;
        kViewport.y = 0.0f;
        kViewport.width = static_cast<float>(frameBuffer->XJGetWidth());
        kViewport.height = static_cast<float>(frameBuffer->XJGetHeight());
        kViewport.minDepth = 0.0f;
        kViewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &kViewport);
        //设置裁剪矩形
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {frameBuffer->XJGetWidth(), frameBuffer->XJGetHeight()};
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        //更新设备  模型 窗口 时间
        XJMaterialRuntimeUploadContext uploadContext = BuildUploadContext(renderTarget);
        const uint32_t frameSlot = uploadContext.FrameSlot % RENDERER_NUM_BUFFER;

        mForceUpdateRuntimes.clear();
        mUpdatedFrameRuntimes.clear();
        mRequiredDescriptorCountByRuntime.clear();

        // 只统计当前 scene 本帧实际要画的材质。仍然用 material index 做 descriptor 下标，
    // 因此 requiredCount 必须是 max(materialIndex)+1，而不是材质个数。
        for (const XJMaterialRenderItem& item : mRenderItems)
        {
            XJMaterial* material = item.Material;
            if (!material || material->XJGetIndex() < 0)
                continue;
        
            XJMaterialPipelineRuntime* runtime = ResolveMaterialRuntime(material);
            if (!runtime || !runtime->IsValid())
                continue;
        
            const uint32_t requiredCount = material->GetIndex() + 1;
            uint32_t& currentRequiredCount = mRequiredDescriptorCountByRuntime[runtime];
        
            if (currentRequiredCount < requiredCount)
                currentRequiredCount = requiredCount;
        }

        if (mRequiredDescriptorCountByRuntime.find(defaultRuntime) == mRequiredDescriptorCountByRuntime.end())
            mRequiredDescriptorCountByRuntime[defaultRuntime] = NUM_MATERIAL_BATCH;

        for (auto& [runtime, requiredCount] : mRequiredDescriptorCountByRuntime)
        {
            if (requiredCount < NUM_MATERIAL_BATCH)
                requiredCount = NUM_MATERIAL_BATCH;
        
            if (requiredCount > runtime->LastDescriptorSetCount)
            {
                spdlog::debug(
                    "Unlit: runtime pool resize before command recording, shader='{}', count={} -> {}",
                    runtime->ShaderLayout.ShaderPath.generic_string(),
                    runtime->LastDescriptorSetCount,
                    requiredCount);
                
                if (!ReCreateMaterialDescPool(*runtime, requiredCount))
                {
                    spdlog::error(
                        "Unlit: failed to resize material descriptor pool, shader='{}'.",
                        runtime->ShaderLayout.ShaderPath.generic_string());
                    return;
                }
            
                // descriptor pool 重建后所有 material/frame slot 都需要重新写 descriptor。
                mForceUpdateRuntimes.insert(runtime);
                mParamUploadedByRuntime[runtime].assign(requiredCount * RENDERER_NUM_BUFFER, false);
                mResourceUploadedByRuntime[runtime].assign(requiredCount * RENDERER_NUM_BUFFER, false);
            }
             else
            {
                const uint32_t flagCount = runtime->LastDescriptorSetCount * RENDERER_NUM_BUFFER;
                mParamUploadedByRuntime[runtime].resize(flagCount, false);
                mResourceUploadedByRuntime[runtime].resize(flagCount, false);
            }
        }

        std::unordered_map<XJMaterialPipelineRuntime*, std::unordered_set<uint32_t>> preparedMaterialIndicesByRuntime;

        for (const XJMaterialRenderItem& item : mRenderItems)
        {
            XJMaterial* material = item.Material;
            if (!material || material->XJGetIndex() < 0 || !item.Mesh)
                continue;

            XJMaterialPipelineRuntime* runtime = ResolveMaterialRuntime(material);
            if (!runtime || !runtime->IsValid())
                continue;

            if (mUpdatedFrameRuntimes.insert(runtime).second)
                XJMaterialRuntimeUploader::UpdateFrameUboDescSet(uploadContext, *runtime);

            const uint32_t materialIndex = material->GetIndex();
            const uint32_t descriptorIndex = frameSlot * runtime->LastDescriptorSetCount + materialIndex;

            if (materialIndex >= runtime->LastDescriptorSetCount ||
                descriptorIndex >= runtime->MaterialParamDescSets.size() ||
                descriptorIndex >= runtime->MaterialResourceDescSets.size())
            {
                continue;
            }

            auto& preparedMaterialIndices = preparedMaterialIndicesByRuntime[runtime];
            if (!preparedMaterialIndices.insert(materialIndex).second)
                continue;

            const bool forceUpdateRuntime = mForceUpdateRuntimes.find(runtime) != mForceUpdateRuntimes.end();

            if (material->ShouldFlushParams())
                MarkMaterialParamsDirtyForAllFrameSlots(*runtime, materialIndex);

            if (material->ShouldFlushResoure())
                MarkMaterialResourcesDirtyForAllFrameSlots(*runtime, materialIndex);

            std::vector<bool>& paramFlags = mParamUploadedByRuntime[runtime];
            std::vector<bool>& resourceFlags = mResourceUploadedByRuntime[runtime];

            VkDescriptorSet paramsDescSet = runtime->MaterialParamDescSets[descriptorIndex];
            VkDescriptorSet resourceDescSet = runtime->MaterialResourceDescSets[descriptorIndex];

            // Descriptor sets must be updated before any draw in this command buffer
            // can bind them. Updating a set after it was bound invalidates recording.
            if (forceUpdateRuntime || descriptorIndex >= paramFlags.size() || !paramFlags[descriptorIndex])
            {
                if (XJMaterialRuntimeUploader::UpdateMaterialParamsDescSet(
                        uploadContext.Device,
                        *runtime,
                        paramsDescSet,
                        descriptorIndex,
                        material))
                {
                    if (descriptorIndex < paramFlags.size())
                        paramFlags[descriptorIndex] = true;
                }

                if (!HasPendingMaterialParamUpdates(*runtime, materialIndex))
                    material->FinishFlushParams();
            }

            if (forceUpdateRuntime || descriptorIndex >= resourceFlags.size() || !resourceFlags[descriptorIndex])
            {
                if (XJMaterialRuntimeUploader::UpdateMaterialResourceDescSet(
                        uploadContext.Device,
                        *runtime,
                        resourceDescSet,
                        material))
                {
                    if (descriptorIndex < resourceFlags.size())
                        resourceFlags[descriptorIndex] = true;
                }

                if (!HasPendingMaterialResourceUpdates(*runtime, materialIndex))
                    material->FinishFlushResoure();
            }
        }

        XJMaterialPipelineRuntime* boundRuntime = nullptr;
        mUpdatedFrameRuntimes.clear();
        
        for (const XJMaterialRenderItem& item : mRenderItems)
        {
            XJMaterial* material = item.Material;
            if (!material || material->XJGetIndex() < 0 || !item.Mesh)
                continue;
            
            XJMaterialPipelineRuntime* runtime = ResolveMaterialRuntime(material);
            if (!runtime || !runtime->IsValid())
            {
                spdlog::warn(
                    "Skip material {}: failed to resolve valid pipeline runtime.",
                    material->GetIndex());
                continue;
            }
        
            if (boundRuntime != runtime)
            {
                runtime->Pipeline->BindPipeline(cmdBuffer);
                boundRuntime = runtime;
            }
        
            const uint32_t materialIndex = material->GetIndex();
            const uint32_t descriptorIndex = frameSlot * runtime->LastDescriptorSetCount + materialIndex;
        
            if (materialIndex >= runtime->LastDescriptorSetCount ||
                descriptorIndex >= runtime->MaterialParamDescSets.size() ||
                descriptorIndex >= runtime->MaterialResourceDescSets.size())
            {
                spdlog::warn(
                    "Skip material {}: descriptor set index is out of bounds.",
                    material->GetIndex());
                continue;
            }

            
        
            VkDescriptorSet paramsDescSet = runtime->MaterialParamDescSets[descriptorIndex];
            VkDescriptorSet resourceDescSet = runtime->MaterialResourceDescSets[descriptorIndex];

            VkDescriptorSet descriptorSets[] =
            {
                runtime->FrameUboDescSets[frameSlot],
                paramsDescSet,
                resourceDescSet
            };

            vkCmdBindDescriptorSets(
                cmdBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                runtime->PipelineLayout->XJGetPipelineLayout(),
                0,
                ARRAY_SIZE(descriptorSets),
                descriptorSets,
                0,
                nullptr);

            ModelPC pc{};
            pc.modelMat = item.ModelMatrix;

            // normalMat 必须是 model 的逆转置；否则非等比缩放下法线错误。
            // 用 mat4 上传是为了匹配 shader push constant 布局，避免 glm::mat3 的紧凑内存布局错位。
            const glm::mat3 normalMat3 = glm::inverseTranspose(glm::mat3(item.ModelMatrix));
            pc.normalMat[0] = glm::vec4(normalMat3[0], 0.0f);
            pc.normalMat[1] = glm::vec4(normalMat3[1], 0.0f);
            pc.normalMat[2] = glm::vec4(normalMat3[2], 0.0f);
            pc.normalMat[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

            vkCmdPushConstants(
                cmdBuffer,
                runtime->PipelineLayout->XJGetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(pc),
                &pc);

            item.Mesh->Draw(cmdBuffer);
        }
        
    }

    void XJUnlitMaterialSystem::OnDestroy() 
    {
        mRenderItems.clear();
        mForceUpdateRuntimes.clear();
        mUpdatedFrameRuntimes.clear();
        mRequiredDescriptorCountByRuntime.clear();
        mParamUploadedByRuntime.clear();
        mResourceUploadedByRuntime.clear();
        
        ShutdownMaterialRuntime();
    }
}
