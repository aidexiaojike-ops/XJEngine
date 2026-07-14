#ifndef XJ_SHADER_DESCRIPTOR_LAYOUT_BUILDER_H
#define XJ_SHADER_DESCRIPTOR_LAYOUT_BUILDER_H

#include "Render/Shader/XJShaderReflection.h"
#include "Graphic/VulkanCommon.h"

#include <vector>

namespace XJ
{
    inline VkShaderStageFlags ToVkShaderStageFlags(XJShaderStage stage)
    {
        switch (stage)
        {
            case XJShaderStage::Vertex:
                return VK_SHADER_STAGE_VERTEX_BIT;

            case XJShaderStage::Fragment:
                return VK_SHADER_STAGE_FRAGMENT_BIT;

            case XJShaderStage::Compute:
                return VK_SHADER_STAGE_COMPUTE_BIT;

            default:
                return VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        }
    }

    inline VkDescriptorType ToVkDescriptorType(XJShaderDescriptorType type)
    {
        switch (type)
        {
            case XJShaderDescriptorType::UniformBuffer:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

            case XJShaderDescriptorType::CombinedImageSampler:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

            case XJShaderDescriptorType::Sampler:
                return VK_DESCRIPTOR_TYPE_SAMPLER;

            case XJShaderDescriptorType::SampledImage:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

            default:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
    }

    inline void AddOrMergeDescriptorBinding(
        std::vector<VkDescriptorSetLayoutBinding>& bindings,
        uint32_t bindingIndex,
        VkDescriptorType descriptorType,
        VkShaderStageFlags stageFlags)
    {
        for (auto& binding : bindings)
        {
            if (binding.binding == bindingIndex)
            {
                binding.stageFlags |= stageFlags;
                return;
            }
        }

        VkDescriptorSetLayoutBinding binding{};
        binding.binding = bindingIndex;
        binding.descriptorType = descriptorType;
        binding.descriptorCount = 1;
        binding.stageFlags = stageFlags;
        binding.pImmutableSamplers = nullptr;

        bindings.push_back(binding);
    }

    inline std::vector<VkDescriptorSetLayoutBinding> BuildDescriptorSetLayoutBindings(
        const XJShaderReflectionResult& reflection,
        uint32_t setIndex)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        for (const auto& ubo : reflection.Ubos)
        {
            if (ubo.Set != setIndex)
                continue;

            AddOrMergeDescriptorBinding(
                bindings,
                ubo.Binding,
                ToVkDescriptorType(ubo.DescriptorType),
                ToVkShaderStageFlags(ubo.Stage));
        }

        for (const auto& sampler : reflection.Samplers)
        {
            if (sampler.Set != setIndex)
                continue;

            AddOrMergeDescriptorBinding(
                bindings,
                sampler.Binding,
                ToVkDescriptorType(sampler.DescriptorType),
                ToVkShaderStageFlags(sampler.Stage));
        }

        return bindings;
    }


    inline void AddDescriptorPoolSize(
        std::vector<VkDescriptorPoolSize>& poolSizes,
        VkDescriptorType type,
        uint32_t count)
    {
        if (count == 0)
            return;
    
        for (auto& poolSize : poolSizes)
        {
            if (poolSize.type == type)
            {
                poolSize.descriptorCount += count;
                return;
            }
        }
    
        VkDescriptorPoolSize poolSize{};
        poolSize.type = type;
        poolSize.descriptorCount = count;
        poolSizes.push_back(poolSize);
    }
    
    inline std::vector<VkDescriptorPoolSize> BuildDescriptorPoolSizes(
        const XJShaderReflectionResult& reflection,
        uint32_t setIndex,
        uint32_t descriptorSetCount)
    {
        std::vector<VkDescriptorPoolSize> poolSizes;
    
        for (const auto& ubo : reflection.Ubos)
        {
            if (ubo.Set != setIndex)
                continue;
        
            AddDescriptorPoolSize(
                poolSizes,
                ToVkDescriptorType(ubo.DescriptorType),
                descriptorSetCount);
        }
    
        for (const auto& sampler : reflection.Samplers)
        {
            if (sampler.Set != setIndex)
                continue;
        
            AddDescriptorPoolSize(
                poolSizes,
                ToVkDescriptorType(sampler.DescriptorType),
                descriptorSetCount);
        }
    
        return poolSizes;
    }
}

#endif