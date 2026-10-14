#ifndef XJ_MATERIAL_PIPELINE_RUNTIME_DESCRIPTOR_H
#define XJ_MATERIAL_PIPELINE_RUNTIME_DESCRIPTOR_H

#include "Render/Material/XJMaterialPipelineRuntime.h"

#include <cstdint>
#include <optional>
//抽取 Descriptor 扩容
namespace XJ
{
    class XJVulkanDevice;

    struct XJMaterialDescriptorAllocationPlan
    {
        uint32_t MaterialCapacity = 0;
        uint32_t DescriptorSetsPerLayout = 0;
        uint32_t MaxSets = 0;
        uint32_t MaterialUboBufferCount = 0;
    };

    class XJMaterialPipelineRuntimeDescriptor
    {
        public:
            static constexpr uint32_t InitialMaterialCapacity = 256;
            static constexpr uint32_t MaximumMaterialCapacity = 4096;

            static std::optional<XJMaterialDescriptorAllocationPlan>
            BuildAllocationPlan(//构建分配计划
                uint32_t currentCapacity,
                uint32_t requiredMaterialCount,
                uint32_t frameSlotCount,
                uint32_t materialUboCount);

            static bool ReCreateMaterialDescPool(//重新descpool
                XJVulkanDevice* device,
                XJMaterialPipelineRuntime& runtime,
                uint32_t materialCount);

    };
}

#endif
