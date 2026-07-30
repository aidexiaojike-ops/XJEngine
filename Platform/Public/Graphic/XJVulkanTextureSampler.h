#ifndef XJ_VULKAN_TEXTURE_SAMPLER_H
#define XJ_VULKAN_TEXTURE_SAMPLER_H

#include "Graphic/VulkanCommon.h"

namespace XJ
{
    class XJVulkanDevice;

    class XJVulkanTextureSampler
    {

        private:
            XJVulkanDevice *mDevice = nullptr;
            VkSampler mSampler = VK_NULL_HANDLE;
            /* data */
        public:
            XJVulkanTextureSampler(
                XJVulkanDevice* device,
                VkFilter filter = VK_FILTER_LINEAR,
                VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
            ~XJVulkanTextureSampler();

            XJVulkanTextureSampler(const XJVulkanTextureSampler&) = delete;
            XJVulkanTextureSampler& operator=(const XJVulkanTextureSampler&) = delete;

            VkSampler XJGetSampler() const { return mSampler; }
            bool IsValid() const { return mSampler != VK_NULL_HANDLE; }
    };
    
    
    
}


#endif