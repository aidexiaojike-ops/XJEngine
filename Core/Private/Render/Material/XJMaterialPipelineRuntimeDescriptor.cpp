#include "Render/Material/XJMaterialPipelineRuntimeDescriptor.h"

#include "Graphic/XJVulkanBuffer.h"
#include "Graphic/XJVulkanDescriptorSet.h"
#include "Render/Shader/XJShaderDescriptorLayoutBuilder.h"
#include <limits>

namespace XJ
{
    std::optional<XJMaterialDescriptorAllocationPlan>
    XJMaterialPipelineRuntimeDescriptor::BuildAllocationPlan(//构建分配计划
        uint32_t currentCapacity,
        uint32_t requiredMaterialCount,
        uint32_t frameSlotCount,
        uint32_t materialUboCount)
    {
        if (frameSlotCount == 0 || currentCapacity > MaximumMaterialCapacity)
            return std::nullopt;

        uint32_t capacity = currentCapacity == 0 ? InitialMaterialCapacity : currentCapacity;

        while (capacity < requiredMaterialCount)
        {
            if (capacity >= MaximumMaterialCapacity ||
                capacity > MaximumMaterialCapacity / 2)
            {
                return std::nullopt;
            }

            capacity *= 2;
        }
        const uint64_t descriptorCount = static_cast<uint64_t>(capacity) * frameSlotCount;//计算描述符数量
        const uint64_t maximumCount = std::numeric_limits<uint32_t>::max();
        if (descriptorCount > maximumCount / 2 ||
            (materialUboCount != 0 && descriptorCount > maximumCount / materialUboCount))
        {
            return std::nullopt;
        }
        const uint64_t maxSets = descriptorCount * 2;
        const uint64_t uboBufferCount = descriptorCount * materialUboCount;

        return XJMaterialDescriptorAllocationPlan{
            capacity,
            static_cast<uint32_t>(descriptorCount),
            static_cast<uint32_t>(maxSets),
            static_cast<uint32_t>(uboBufferCount)
        };
    }

    bool XJMaterialPipelineRuntimeDescriptor::ReCreateMaterialDescPool(
        XJVulkanDevice* device,
        XJMaterialPipelineRuntime& runtime,
        uint32_t materialCount)
    {

        const auto plan = BuildAllocationPlan(
            runtime.LastDescriptorSetCount,
            materialCount,
            RENDERER_NUM_BUFFER,
            static_cast<uint32_t>(
                runtime.ShaderLayout.MaterialUboLayouts.size()));

        if (!plan)
        {
            spdlog::error(
                "Descriptor pool capacity exceeded, request={}",
                materialCount);
            return false;
        }
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
        const uint32_t newDescriptorSetCount = plan->MaterialCapacity;
        // 其他 frame slot 可能仍在使用旧 pool/buffer。扩容很少发生，先确保 GPU 已完成访问。
        if (runtime.MaterialDescriptorPool)
            device->WaitIdle();

        //销毁老参数
        runtime.MaterialParamDescSets.clear();
        runtime.MaterialResourceDescSets.clear();
        runtime.MaterialDescriptorPool.reset();
        //重新申请池子
        std::vector<VkDescriptorPoolSize> poolSizes;

        const uint32_t totalDescriptorSetCount = plan->DescriptorSetsPerLayout;

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
                plan->MaxSets,
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
        runtime.MaterialUboBuffers.resize(plan->MaterialUboBufferCount);
        runtime.MaterialUboBufferSizes.resize(plan->MaterialUboBufferCount, 0);
        runtime.LastDescriptorSetCount = newDescriptorSetCount;

        return true;
    }
}
