#include "Render/Material/XJMaterialPipelineRuntimeDescriptor.h"

#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanDescriptorSet.h"
#include "Render/Shader/XJShaderDescriptorLayoutBuilder.h"

namespace XJ
{
    namespace
    {
        constexpr uint32_t MATERIAL_BATCH = 256;
        constexpr uint32_t MATERIAL_BATCH_MAX = 4096;
    }

    bool XJMaterialPipelineRuntimeDescriptor::ReCreateMaterialDescPool(
        XJVulkanDevice* device,
        XJMaterialPipelineRuntime& runtime,
        uint32_t materialCount)
    {
        if (!device)
        {
            spdlog::error("ReCreateMaterialDescPool failed: device is null.");
            return false;
        }

        if (!runtime.IsValid())
        {
            spdlog::error("ReCreateMaterialDescPool failed: pipeline runtime is invalid.");
            return false;
        }
        //最新池子要需要放多少个
        uint32_t newDescriptorSetCount = runtime.LastDescriptorSetCount;
        if (runtime.LastDescriptorSetCount == 0)
            newDescriptorSetCount = MATERIAL_BATCH;

        while (newDescriptorSetCount < materialCount)
        {
            newDescriptorSetCount *= 2;//2倍数增长   直到大于材质数量
            spdlog::debug("ReCreateMaterialDescPool, new Descriptor Set count: {}", newDescriptorSetCount);
        }

        if (newDescriptorSetCount > MATERIAL_BATCH_MAX)//大于最大的数量就报错
        {
            spdlog::error(
                "Descriptor Set max count is:{},but request:{}",
                MATERIAL_BATCH_MAX,
                newDescriptorSetCount);
            return false;
        }
        //销毁老参数
        runtime.MaterialParamDescSets.clear();
        runtime.MaterialResourceDescSets.clear();
        runtime.MaterialDescriptorPool.reset();
        //重新申请池子
        std::vector<VkDescriptorPoolSize> poolSizes;

        auto paramPoolSizes = BuildDescriptorPoolSizes(
            runtime.ShaderLayout.Reflection,
            runtime.ShaderLayout.MaterialParameterSet,
            newDescriptorSetCount);

        auto resourcePoolSizes = BuildDescriptorPoolSizes(
            runtime.ShaderLayout.Reflection,
            runtime.ShaderLayout.MaterialResourceSet,
            newDescriptorSetCount);

        poolSizes.insert(poolSizes.end(), paramPoolSizes.begin(), paramPoolSizes.end());

        for (const auto& poolSize : resourcePoolSizes)
            AddDescriptorPoolSize(poolSizes, poolSize.type, poolSize.descriptorCount);
        //申请材质
        runtime.MaterialDescriptorPool =
            std::make_shared<XJ::XJVulkanDescriptorPool>(
                device,
                newDescriptorSetCount * 2,
                poolSizes);

        runtime.MaterialParamDescSets =
            runtime.MaterialDescriptorPool->AllocateDescriptorSet(
                runtime.MaterialParamDescSetLayout.get(),
                newDescriptorSetCount);

        runtime.MaterialResourceDescSets =
            runtime.MaterialDescriptorPool->AllocateDescriptorSet(
                runtime.MaterialResourceDescSetLayout.get(),
                newDescriptorSetCount);

        if (runtime.MaterialParamDescSets.size() != newDescriptorSetCount ||
            runtime.MaterialResourceDescSets.size() != newDescriptorSetCount)
        {
            spdlog::error("ReCreateMaterialDescPool failed: descriptor set allocation count mismatch.");
            return false;
        }
    // 差值用来创建uBuffer
        runtime.MaterialBuffers.resize(newDescriptorSetCount);
        runtime.MaterialBufferSizes.resize(newDescriptorSetCount, 0);
        runtime.LastDescriptorSetCount = newDescriptorSetCount;

        return true;
    }

    bool XJMaterialPipelineRuntimeDescriptor::EnsureMaterialBuffer(
        XJVulkanDevice* device,
        XJMaterialPipelineRuntime& runtime,
        uint32_t materialIndex,
        uint32_t requiredSize)
    {
        if (requiredSize == 0)
            return true;

        if (!device)
        {
            spdlog::error("EnsureMaterialBuffer failed: device is null.");
            return false;
        }

        if (materialIndex >= runtime.MaterialBuffers.size())
        {
            spdlog::error(
                "Material index {} is out of bounds (max {}).",
                materialIndex,
                runtime.MaterialBuffers.size());
            return false;
        }

        if (runtime.MaterialBuffers[materialIndex] &&
            runtime.MaterialBufferSizes[materialIndex] == requiredSize)
        {
            return true;
        }

        runtime.MaterialBuffers[materialIndex] =
            std::make_shared<XJVulkanBuffer>(
                device,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                requiredSize,
                nullptr,
                true);

        runtime.MaterialBufferSizes[materialIndex] = requiredSize;
        return true;
    }
}
