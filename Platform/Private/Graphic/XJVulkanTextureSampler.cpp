#include "Graphic/XJVulkanTextureSampler.h"
#include "Graphic/XJVulkanDevice.h"
#include <stdexcept>

namespace XJ
{
    XJVulkanTextureSampler::XJVulkanTextureSampler(
        XJVulkanDevice* device,
        VkFilter filter,
        VkSamplerAddressMode addressMode)
        : mDevice(device)
    {
        if (!mDevice || !mDevice->IsValid())
        {
            spdlog::error("XJVulkanTextureSampler failed: device is invalid");
            throw std::runtime_error("XJVulkanTextureSampler failed: device is invalid");
        }

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.pNext = nullptr;
        samplerInfo.flags = 0;
        samplerInfo.magFilter = filter;
        samplerInfo.minFilter = filter;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = addressMode;
        samplerInfo.addressModeV = addressMode;
        samplerInfo.addressModeW = addressMode;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        VkResult result = vkCreateSampler(mDevice->XJGetDevice(), &samplerInfo, nullptr, &mSampler);
        XJDebug_Log(result);

        if (result != VK_SUCCESS)
        {
            spdlog::error("XJVulkanTextureSampler failed: vkCreateSampler failed");
            throw std::runtime_error("XJVulkanTextureSampler failed: vkCreateSampler failed");
        }
    }

    XJVulkanTextureSampler::~XJVulkanTextureSampler()
    {
        if (mDevice && mDevice->IsValid() && mSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(mDevice->XJGetDevice(), mSampler, nullptr);
            mSampler = VK_NULL_HANDLE;
        }
    }

} 