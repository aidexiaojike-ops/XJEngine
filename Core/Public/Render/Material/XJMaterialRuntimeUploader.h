#ifndef XJ_MATERIAL_RUNTIME_UPLOADER_H
#define XJ_MATERIAL_RUNTIME_UPLOADER_H

#include "Graphic/VulkanCommon.h"
#include "Render/Material/XJMaterialPipelineRuntime.h"

#include <glm/glm.hpp>

namespace XJ
{
    class XJVulkanDevice;
    class XJMaterial;

    struct XJMaterialRuntimeUploadContext
    {
        XJVulkanDevice* Device = nullptr;

        glm::mat4 ProjMat{1.0f};
        glm::mat4 ViewMat{1.0f};
        glm::ivec2 Resolution{0, 0};

        uint32_t FrameId = 0;
        float Time = 0.0f;
    };

    class XJMaterialRuntimeUploader
    {
        public:
            static bool UpdateFrameUboDescSet(
                const XJMaterialRuntimeUploadContext& context,
                XJMaterialPipelineRuntime& runtime);

            static bool UpdateMaterialParamsDescSet(
                XJVulkanDevice* device,
                XJMaterialPipelineRuntime& runtime,
                VkDescriptorSet descSet,
                XJMaterial* material);

            static bool UpdateMaterialResourceDescSet(
                XJVulkanDevice* device,
                XJMaterialPipelineRuntime& runtime,
                VkDescriptorSet descSet,
                XJMaterial* material);
    };
}

#endif
