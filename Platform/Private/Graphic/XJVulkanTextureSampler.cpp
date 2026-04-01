#include "Graphic/XJVulkanTextureSampler.h"
#include "Graphic/XJVulkanDevice.h"

namespace XJ
{
    VkResult XJVulkanTextureSampler::CreateSimpleSampler(XJVulkanDevice* device, VkFilter filter, VkSamplerAddressMode addressMode, VkSampler *outSampler)
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.pNext = nullptr;
        samplerInfo.flags = 0;
        samplerInfo.magFilter = filter;
        samplerInfo.minFilter = filter;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;//MIPMAP
        samplerInfo.addressModeU = addressMode;
        samplerInfo.addressModeV = addressMode;
        samplerInfo.addressModeW = addressMode;
        samplerInfo.mipLodBias = 0;//MIPMAP DENG JI
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.minLod = 0;
        samplerInfo.maxLod = 1;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        return vkCreateSampler(device->XJGetDevice(), &samplerInfo, nullptr, outSampler);
    }

} 