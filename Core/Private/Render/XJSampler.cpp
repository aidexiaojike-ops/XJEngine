#include "Render/XJSampler.h"

#include "XJApplication.h"
#include "Graphic/XJVulkanTextureSampler.h"

namespace XJ
{
    XJSampler::XJSampler(VkFilter filter, VkSamplerAddressMode addressMode)
    : mFilter(filter), mAddressMode(addressMode), mTextureSampler(std::make_shared<XJVulkanTextureSampler>())
    {
    
        XJ::XJRenderContext *kRenderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice  = kRenderContext->XJGetDevice();
      
        XJDebug_Log(mTextureSampler->CreateSimpleSampler(kDevice, mFilter, mAddressMode, &mSampler));
        
    }
    XJSampler::~XJSampler()
    {
        XJ::XJRenderContext *renderContext = XJApplication::XJGetAppContext()->renderContext;
        XJ::XJVulkanDevice *kDevice  = renderContext->XJGetDevice();
        if (mSampler != VK_NULL_HANDLE) {
            XJ::XJRenderContext *renderContext = XJApplication::XJGetAppContext()->renderContext;
            XJ::XJVulkanDevice *kDevice  = renderContext->XJGetDevice();
            vkDestroySampler(kDevice->XJGetDevice(), mSampler, nullptr);
            mSampler = VK_NULL_HANDLE;
        }

    }
}