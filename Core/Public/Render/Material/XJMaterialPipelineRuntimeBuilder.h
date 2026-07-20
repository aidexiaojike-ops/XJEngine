#ifndef XJ_MATERIAL_PIPELINE_RUNTIME_BUILDER_H
#define XJ_MATERIAL_PIPELINE_RUNTIME_BUILDER_H

#include "Graphic/VulkanCommon.h"
#include "Render/Material/XJMaterialPipelineRuntime.h"

namespace XJ
{

    class XJVulkanDevice;
    class XJVulkanRenderPass;
    class XJShaderAsset;

    struct XJMaterialPipelineRuntimeBuildContext//逻辑设备 渲染通道  采样等级
    {
        XJVulkanDevice* Device = nullptr;
        XJVulkanRenderPass* RenderPass = nullptr;
        VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT;
    };

    class XJMaterialPipelineRuntimeBuilder
    {
        public:
            static bool Build(
                const XJShaderAsset& shaderAsset,
                const XJMaterialPipelineRuntimeBuildContext& context,
                XJMaterialPipelineRuntime& outRuntime);

        private:
            static std::string ToPipelineShaderSourcePath(const std::filesystem::path& shaderPath);
    };

}


#endif
