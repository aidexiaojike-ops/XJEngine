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

#include <unordered_map>
#include <unordered_set>
#include <vector>
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
    void XJUnlitMaterialSystem::OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) 
    {
        XJScene *kScene = XJGetScene();

        if(!kScene){return;}//如果场景不存在，直接返回

        XJMaterialPipelineRuntime* defaultRuntime = GetDefaultMaterialRuntime();
        if (!defaultRuntime  || !defaultRuntime->IsValid())
        {
            spdlog::error("Unlit material system has no valid default pipeline runtime.");
            return;
        }

        std::vector<XJMaterialRenderItem> renderItems = XJUnlitMaterialRenderItemBuilder::Build(*kScene);
        if (renderItems.empty())
            return;// 视图确实为空
      
         //bind pipeline
        //runtime->Pipeline->BindPipeline(cmdBuffer);//绑定管线
        //vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->XJGetPipelineLayout(), 0, 1,  mDescriptorSets.data(), 0, nullptr);
        XJ::XJVulkanFrameBuffer *kFrameBuffer = renderTarget->XJGetCurrentFrameBuffer();
        if (!kFrameBuffer) 
        {
            spdlog::error("FrameBuffer is null, skipping render");
            return;
        }
        //设置视口和裁剪矩形
        VkViewport kViewport{};
        kViewport.x = 0.0f;
        kViewport.y = 0.0f;
        kViewport.width = static_cast<float>(kFrameBuffer->XJGetWidth());
        kViewport.height = static_cast<float>(kFrameBuffer->XJGetHeight());
        kViewport.minDepth = 0.0f;
        kViewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &kViewport);
        //设置裁剪矩形
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {kFrameBuffer->XJGetWidth(), kFrameBuffer->XJGetHeight()};
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        //更新设备  模型 窗口 时间
        XJMaterialRuntimeUploadContext uploadContext = BuildUploadContext(renderTarget);

        uint32_t kMaterialCount = XJMaterialFactory::GetInstance()->GetMaterialSize<XJUnlitMaterial>();//材质数量
        std::unordered_set<XJMaterialPipelineRuntime*> forceUpdateRuntimes;

        std::unordered_map<XJMaterialPipelineRuntime*, uint32_t> requiredDescriptorCountByRuntime;
        //预扫描
        for (const XJMaterialRenderItem& item : renderItems)
        {
            XJMaterial* material = item.Material;
            if (!material || material->XJGetIndex() < 0)
                continue;
        
            XJMaterialPipelineRuntime* runtime = ResolveMaterialRuntime(material);
            if (!runtime || !runtime->IsValid())
                continue;
        
            const uint32_t requiredCount = material->GetIndex() + 1;
            uint32_t& currentRequiredCount = requiredDescriptorCountByRuntime[runtime];
        
            if (currentRequiredCount < requiredCount)
                currentRequiredCount = requiredCount;
        }

        if (requiredDescriptorCountByRuntime.find(defaultRuntime) == requiredDescriptorCountByRuntime.end())
            requiredDescriptorCountByRuntime[defaultRuntime] = kMaterialCount;

        for (auto& [runtime, requiredCount] : requiredDescriptorCountByRuntime)
        {
            if (requiredCount < kMaterialCount)
                requiredCount = kMaterialCount;
        
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
            
                forceUpdateRuntimes.insert(runtime);
            }
        }

        XJMaterialPipelineRuntime* boundRuntime = nullptr;
        std::unordered_set<XJMaterialPipelineRuntime*> updatedFrameRuntimes;
        //材质是否更新
        std::unordered_map<XJMaterialPipelineRuntime*, std::vector<bool>> updateFlagsByRuntime;
        const uint32_t frameSlot = uploadContext.FrameSlot % RENDERER_NUM_BUFFER;

        for (const XJMaterialRenderItem& item : renderItems)
        {
            XJMaterial* material = item.Material;
            if (!material || material->XJGetIndex() < 0)
            {
                spdlog::error("TODO: Default material of error material ?");
                continue;
            }
                        
            if (!item.Mesh)
            {
                continue;
            }

            
        
            const uint32_t materialIndex = material->GetIndex();
        
            XJMaterialPipelineRuntime* runtime = ResolveMaterialRuntime(material);
        
            if (!runtime || !runtime->IsValid())
            {
                spdlog::warn(
                    "Skip material {}: failed to resolve valid pipeline runtime.",
                    material->GetIndex());
                continue;
            }
        
            if (updatedFrameRuntimes.insert(runtime).second)
                XJMaterialRuntimeUploader::UpdateFrameUboDescSet(uploadContext, *runtime);
        
            if (boundRuntime != runtime)
            {
                runtime->Pipeline->BindPipeline(cmdBuffer);
                boundRuntime = runtime;
            }
            const uint32_t descriptorIndex = frameSlot * runtime->LastDescriptorSetCount + materialIndex;

            // 每个 frame slot 使用独立的材质 descriptor set，避免更新 pending command buffer 正在使用的 set。
            if (materialIndex >= runtime->LastDescriptorSetCount ||
                descriptorIndex >= runtime->MaterialParamDescSets.size() ||
                descriptorIndex >= runtime->MaterialResourceDescSets.size())
            {
                spdlog::warn(
                    "Skip material {}: descriptor set index is out of bounds.",
                    material->GetIndex());
                continue;
            }
        
            auto& runtimeUpdateFlags = updateFlagsByRuntime[runtime];
            const uint32_t runtimeUpdateFlagCount = runtime->LastDescriptorSetCount * RENDERER_NUM_BUFFER;
            if (runtimeUpdateFlags.size() < runtimeUpdateFlagCount)
                runtimeUpdateFlags.resize(runtimeUpdateFlagCount, false);
        
            const bool forceUpdateRuntime =
                forceUpdateRuntimes.find(runtime) != forceUpdateRuntimes.end();
        
            VkDescriptorSet paramsDescSet = runtime->MaterialParamDescSets[descriptorIndex];
            VkDescriptorSet resourceDescSet = runtime->MaterialResourceDescSets[descriptorIndex];
        
            if (!runtimeUpdateFlags[descriptorIndex] || forceUpdateRuntime)
            {
                XJMaterialRuntimeUploader::UpdateMaterialParamsDescSet(
                    uploadContext.Device,
                    *runtime,
                    paramsDescSet,
                    descriptorIndex,
                    material);
                
                material->FinishFlushParams();
                
                XJMaterialRuntimeUploader::UpdateMaterialResourceDescSet(
                    uploadContext.Device,
                    *runtime,
                    resourceDescSet,
                    material);
                
                material->FinishFlushResoure();
                
                runtimeUpdateFlags[descriptorIndex] = true;
            }
        
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
            
            ModelPC pc = { item.ModelMatrix };
            
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
        ShutdownMaterialRuntime();
    }
}
