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
            /* data */
        public:
            //XJVulkanTextureSampler(/* args */);
            //~XJVulkanTextureSampler();
            VkResult CreateSimpleSampler(XJVulkanDevice* device, VkFilter filter, VkSamplerAddressMode addressMode, VkSampler *outSampler);
    };
    
    
    
}


#endif