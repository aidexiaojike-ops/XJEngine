#ifndef XJ_UNLIT_MATERIAL_SYSTEM_H
#define XJ_UNLIT_MATERIAL_SYSTEM_H

#include "Render/System/XJMaterialSystem.h"
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "Render/Material/XJMaterialPipelineRuntime.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace XJ
{
    class XJVulkanDescriptorPool;
    class XJVulkanFrameBuffer;

    class XJUnlitMaterialSystem : public XJMaterialSystem
    {
        public:
            virtual void OnInit(XJVulkanRenderPass *renderPass) override;
            virtual void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) override;
            virtual void OnDestroy() override;

            VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT;// 多重采样数量  minmap levels

        private:
            void ReCreateMaterialDescPool(XJMaterialPipelineRuntime& runtime, uint32_t materialCount);

            void UpdateFrameUboDescSet(XJMaterialPipelineRuntime& runtime, XJRenderTarget *renderTarget);
            void UpdateMaterialParamsDescSet(
                XJMaterialPipelineRuntime& runtime,
                VkDescriptorSet descSet,
                XJUnlitMaterial *material);
            void UpdateMaterialResourceDescSet(
                XJMaterialPipelineRuntime& runtime,
                VkDescriptorSet descSet,
                XJUnlitMaterial *material);

            XJMaterialPipelineRuntime* GetOrCreatePipelineRuntime(
                const std::filesystem::path& shaderPath,
                XJVulkanRenderPass* renderPass);

            XJMaterialPipelineRuntime* ResolveMaterialPipelineRuntime(
                const XJUnlitMaterial* material);

            XJVulkanRenderPass* mRenderPass = nullptr;

            std::unordered_map<std::string, XJMaterialPipelineRuntime> mPipelineRuntimes;
            XJMaterialPipelineRuntime* mDefaultPipelineRuntime = nullptr;
            std::unordered_set<std::string> mWarnedFallbackShaderPaths;
  

            void EnsureMaterialBuffer(XJMaterialPipelineRuntime& runtime, uint32_t materialIndex, uint32_t requiredSize);//确保材质缓冲区的大小足够


    };
}

#endif
