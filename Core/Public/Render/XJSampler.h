#ifndef XJ_SAMPLER_H
#define XJ_SAMPLER_H

#include "Graphic/VulkanCommon.h"

namespace XJ
{
    class XJVulkanTextureSampler;

    class XJSampler
    {
        private:
            std::shared_ptr<XJVulkanTextureSampler> mTextureSampler;

        public:
            XJSampler(
                VkFilter filter = VK_FILTER_LINEAR,
                VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
            ~XJSampler();

            VkSampler XJGetSampler() const;
            bool IsValid() const;
    };
}

#endif