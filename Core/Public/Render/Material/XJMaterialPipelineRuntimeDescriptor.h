#ifndef XJ_MATERIAL_PIPELINE_RUNTIME_DESCRIPTOR_H
#define XJ_MATERIAL_PIPELINE_RUNTIME_DESCRIPTOR_H

#include "Render/Material/XJMaterialPipelineRuntime.h"

#include <cstdint>

namespace XJ
{
    class XJVulkanDevice;

    class XJMaterialPipelineRuntimeDescriptor
    {
        public:
            static bool ReCreateMaterialDescPool(//重新descpool
                XJVulkanDevice* device,
                XJMaterialPipelineRuntime& runtime,
                uint32_t materialCount);

            static bool EnsureMaterialBuffer(//验证材质
                XJVulkanDevice* device,
                XJMaterialPipelineRuntime& runtime,
                uint32_t materialIndex,
                uint32_t requiredSize);
    };
}

#endif