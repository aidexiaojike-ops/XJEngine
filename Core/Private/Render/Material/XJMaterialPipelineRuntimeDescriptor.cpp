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

        const uint32_t totalDescriptorSetCount = newDescriptorSetCount * RENDERER_NUM_BUFFER;

        auto paramPoolSizes = BuildDescriptorPoolSizes(
            runtime.ShaderLayout.Reflection,
            runtime.ShaderLayout.MaterialParameterSet,
            totalDescriptorSetCount);

        auto resourcePoolSizes = BuildDescriptorPoolSizes(
            runtime.ShaderLayout.Reflection,
            runtime.ShaderLayout.MaterialResourceSet,
            totalDescriptorSetCount);

        poolSizes.insert(poolSizes.end(), paramPoolSizes.begin(), paramPoolSizes.end());

        for (const auto& poolSize : resourcePoolSizes)
            AddDescriptorPoolSize(poolSizes, poolSize.type, poolSize.descriptorCount);
        //申请材质
        runtime.MaterialDescriptorPool =
            std::make_shared<XJ::XJVulkanDescriptorPool>(
                device,
                totalDescriptorSetCount * 2,
                poolSizes);

        runtime.MaterialParamDescSets =
                runtime.MaterialDescriptorPool->AllocateDescriptorSet(
                runtime.MaterialParamDescSetLayout.get(),
                totalDescriptorSetCount);

        runtime.MaterialResourceDescSets =
                runtime.MaterialDescriptorPool->AllocateDescriptorSet(
                runtime.MaterialResourceDescSetLayout.get(),
                totalDescriptorSetCount);

        if (runtime.MaterialParamDescSets.size() != totalDescriptorSetCount ||
            runtime.MaterialResourceDescSets.size() != totalDescriptorSetCount)
        {
            spdlog::error("ReCreateMaterialDescPool failed: descriptor set allocation count mismatch.");
            return false;
        }
        // 材质 UBO buffer 也按 frame slot 展开，避免当前帧写入覆盖其他 pending 帧读取的数据。
        runtime.MaterialBuffers.resize(totalDescriptorSetCount);
        runtime.MaterialBufferSizes.resize(totalDescriptorSetCount, 0);
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
