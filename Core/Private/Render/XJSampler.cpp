#include "Render/XJSampler.h"

#include "XJApplication.h"
#include "Render/XJRenderContext.h"
#include "Graphic/XJVulkanTextureSampler.h"

#include <spdlog/spdlog.h>

namespace XJ
{
    XJSampler::XJSampler(VkFilter filter, VkSamplerAddressMode addressMode)
    {
        XJRenderContext* renderContext = XJApplication::XJGetAppContext()->renderContext;
        XJVulkanDevice* device = renderContext ? renderContext->XJGetDevice() : nullptr;
        if (!device || !device->IsValid())
        {
            spdlog::error("XJSampler create failed: device is invalid");
            return;
        }

        mTextureSampler = std::make_shared<XJVulkanTextureSampler>(device, filter, addressMode);
    }

    XJSampler::~XJSampler() = default;

    VkSampler XJSampler::XJGetSampler() const
    {
        return mTextureSampler ? mTextureSampler->XJGetSampler() : VK_NULL_HANDLE;
    }

    bool XJSampler::IsValid() const
    {
        return mTextureSampler && mTextureSampler->IsValid();
    }
}