#include "Render/XJEditorRenderResources.h"

#include "Graphic/XJVulkanDevice.h"
#include "Render/Resource/XJTexture.h"
#include "Render/XJRenderContext.h"
#include "Render/XJSampler.h"

#include <spdlog/spdlog.h>

namespace XJ
{
    XJEditorRenderResources::XJEditorRenderResources() = default;

    XJEditorRenderResources::~XJEditorRenderResources()
    {
        Shutdown();
    }

    bool XJEditorRenderResources::Init(XJRenderContext& renderContext)
    {
        Shutdown();

        XJVulkanDevice* device = renderContext.XJGetDevice();

        if (!device || !device->IsValid())
        {
            spdlog::error(
                "Editor render resources initialization "
                "failed: device is invalid.");
            return false;
        }

        mRenderContext = &renderContext;

        RGBAColor whitePixel{255, 255, 255, 255};

        mDefaultTexture = std::make_shared<XJTexture>(1, 1, &whitePixel);

        if (!mDefaultTexture ||
            !mDefaultTexture->IsValid())
        {
            spdlog::error("Editor default texture creation failed.");
            Shutdown();
            return false;
        }

        mDefaultSampler = std::make_shared<XJSampler>(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);

        if (!mDefaultSampler || !mDefaultSampler->IsValid())
        {
            spdlog::error("Editor default sampler creation failed.");
            Shutdown();
            return false;
        }

        mInitialized = true;
        return true;
    }

    const std::shared_ptr<XJTexture>& XJEditorRenderResources::GetDefaultTexture() const
    {
        return mDefaultTexture;
    }

    const std::shared_ptr<XJSampler>& XJEditorRenderResources::GetDefaultSampler() const
    {
        return mDefaultSampler;
    }

    bool XJEditorRenderResources::IsInitialized() const
    {
        return mInitialized;
    }

    void XJEditorRenderResources::Shutdown()
    {
        if (!mRenderContext &&
            !mDefaultTexture &&
            !mDefaultSampler)
        {
            return;
        }

        XJVulkanDevice* device = mRenderContext ? mRenderContext->XJGetDevice() : nullptr;

        if (device)
            device->WaitIdle();

        // 两者都依赖 Vulkan Device，必须在 RenderContext 前释放。
        mDefaultSampler.reset();
        mDefaultTexture.reset();

        mRenderContext = nullptr;
        mInitialized = false;
    }
}