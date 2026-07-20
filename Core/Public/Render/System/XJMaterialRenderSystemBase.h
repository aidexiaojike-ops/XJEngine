#ifndef XJ_MATERIAL_RENDER_SYSTEM_BASE_H
#define XJ_MATERIAL_RENDER_SYSTEM_BASE_H

#include "Render/System/XJMaterialSystem.h"
#include "Render/Material/XJMaterialPipelineRuntimeCache.h"
#include "Render/Material/XJMaterialRuntimeUploader.h"

#include <filesystem>

namespace XJ
{
    class XJMaterial;
    
    //通用 Base System
    class XJMaterialRenderSystemBase : public XJMaterialSystem
    {
        public:
            VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT;

        protected:
            bool InitializeMaterialRuntime(
                XJVulkanRenderPass* renderPass,
                const std::filesystem::path& defaultShaderPath);

            void ShutdownMaterialRuntime();

            XJMaterialPipelineRuntime* GetDefaultMaterialRuntime();
            XJMaterialPipelineRuntime* ResolveMaterialRuntime(const XJMaterial* material);

            void ReCreateMaterialDescPool(
                XJMaterialPipelineRuntime& runtime,
                uint32_t materialCount);

            XJMaterialRuntimeUploadContext BuildUploadContext(
                XJRenderTarget* renderTarget) const;

        private:
            XJVulkanRenderPass* mRenderPass = nullptr;
            XJMaterialPipelineRuntimeCache mPipelineRuntimeCache;
    };
}

#endif