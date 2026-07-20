#include "Render/System/XJMaterialRenderSystemBase.h"

#include "Graphic/XJVulkanFrameBuffer.h"
#include "Render/Material/XJMaterialPipelineRuntimeDescriptor.h"
#include "Render/Resource/XJMaterial.h"
#include "Render/XJRenderTarget.h"
#include "XJApplication.h"

namespace XJ
{
    bool XJMaterialRenderSystemBase::InitializeMaterialRuntime(
        XJVulkanRenderPass* renderPass,
        const std::filesystem::path& defaultShaderPath)
    {
        mRenderPass = renderPass;

        XJMaterialPipelineRuntimeCacheContext cacheContext{};
        cacheContext.Device = XJGetDevice();
        cacheContext.RenderPass = renderPass;
        cacheContext.SampleCount = mSampleCount;
        cacheContext.DefaultShaderPath = defaultShaderPath;

        if (!mPipelineRuntimeCache.Initialize(cacheContext))
        {
            spdlog::error(
                "Material render system failed to initialize pipeline runtime cache: defaultShader='{}'.",
                defaultShaderPath.generic_string());
            return false;
        }

        return true;
    }

    void XJMaterialRenderSystemBase::ShutdownMaterialRuntime()
    {
        mPipelineRuntimeCache.Clear();
        mRenderPass = nullptr;
    }

    XJMaterialPipelineRuntime* XJMaterialRenderSystemBase::GetDefaultMaterialRuntime()
    {
        return mPipelineRuntimeCache.GetDefault();
    }

    XJMaterialPipelineRuntime* XJMaterialRenderSystemBase::ResolveMaterialRuntime(
        const XJMaterial* material)
    {
        if (!material)
            return mPipelineRuntimeCache.GetDefault();

        return mPipelineRuntimeCache.Resolve(material->GetShaderPath());
    }

    void XJMaterialRenderSystemBase::ReCreateMaterialDescPool(
        XJMaterialPipelineRuntime& runtime,
        uint32_t materialCount)
    {
        XJMaterialPipelineRuntimeDescriptor::ReCreateMaterialDescPool(
            XJGetDevice(),
            runtime,
            materialCount);
    }

    XJMaterialRuntimeUploadContext XJMaterialRenderSystemBase::BuildUploadContext(
        XJRenderTarget* renderTarget) const
    {
        XJMaterialRuntimeUploadContext context{};
        context.Device = XJGetDevice();
        context.ProjMat = XJGetProjMat(renderTarget);
        context.ViewMat = XJGetViewMat(renderTarget);

        if (renderTarget)
        {
            if (XJVulkanFrameBuffer* frameBuffer = renderTarget->XJGetCurrentFrameBuffer())
            {
                context.Resolution = {
                    static_cast<int>(frameBuffer->XJGetWidth()),
                    static_cast<int>(frameBuffer->XJGetHeight())
                };
            }
        }

        if (XJApplication* app = XJGetApp())
        {
            context.FrameId = static_cast<uint32_t>(app->XJGetFrameIndex());
            context.Time = app->XJGetStartTimeSecond();
        }

        return context;
    }
}