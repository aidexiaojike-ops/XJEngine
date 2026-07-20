#ifndef XJ_MATERIAL_PIPELINE_RUNTIME_CACHE_H
#define XJ_MATERIAL_PIPELINE_RUNTIME_CACHE_H

#include "Graphic/VulkanCommon.h"
#include "Render/Material/XJMaterialPipelineRuntime.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace XJ
{
    class XJVulkanDevice;
    class XJVulkanRenderPass;

    struct XJMaterialPipelineRuntimeCacheContext
    {
        XJVulkanDevice* Device = nullptr;
        XJVulkanRenderPass* RenderPass = nullptr;
        VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT;
        std::filesystem::path DefaultShaderPath;
    };

    class XJMaterialPipelineRuntimeCache
    {
        public:
            bool Initialize(const XJMaterialPipelineRuntimeCacheContext& context);

            XJMaterialPipelineRuntime* GetDefault();
            const XJMaterialPipelineRuntime* GetDefault() const;

            XJMaterialPipelineRuntime* GetOrCreate(const std::filesystem::path& shaderPath);
            XJMaterialPipelineRuntime* Resolve(const std::filesystem::path& shaderPath);

            void Clear();

        private:
            std::string MakeRuntimeKey(const std::filesystem::path& shaderPath) const;

            XJMaterialPipelineRuntimeCacheContext mContext;
            std::unordered_map<std::string, XJMaterialPipelineRuntime> mRuntimes;
            XJMaterialPipelineRuntime* mDefaultRuntime = nullptr;
            std::unordered_set<std::string> mWarnedFallbackShaderPaths;
    };
}

#endif