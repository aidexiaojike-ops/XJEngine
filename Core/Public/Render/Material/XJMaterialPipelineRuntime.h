#ifndef XJ_MATERIAL_PIPELINE_RUNTIME_H
#define XJ_MATERIAL_PIPELINE_RUNTIME_H

#include "Render/Material/XJMaterialShaderRuntimeLayout.h"
#include "Render/XJRenderer.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace XJ
{
    class XJVulkanDescriptorPool;
    class XJVulkanDescriptorSetLayout;
    class XJVulkanPipelineLayout;
    class XJVulkanPipeline;
    class XJVulkanBuffer;

    struct XJMaterialPipelineRuntime
    {
        XJMaterialShaderRuntimeLayout ShaderLayout;
        //shader 里面的三个输入
        std::shared_ptr<XJVulkanDescriptorSetLayout> FrameUboDescSetLayout;
        std::shared_ptr<XJVulkanDescriptorSetLayout> MaterialParamDescSetLayout;
        std::shared_ptr<XJVulkanDescriptorSetLayout> MaterialResourceDescSetLayout;
        //渲染管线
        std::shared_ptr<XJVulkanPipelineLayout> PipelineLayout;
        std::shared_ptr<XJVulkanPipeline> Pipeline;
        //描述符集
        std::shared_ptr<XJVulkanDescriptorPool> FrameDescriptorPool;
        std::array<VkDescriptorSet, RENDERER_NUM_BUFFER> FrameUboDescSets{};
        std::array<std::shared_ptr<XJVulkanBuffer>, RENDERER_NUM_BUFFER> FrameUboBuffers;

        std::shared_ptr<XJVulkanDescriptorPool> MaterialDescriptorPool;
        uint32_t LastDescriptorSetCount = 0;

        std::vector<VkDescriptorSet> MaterialParamDescSets;
        std::vector<VkDescriptorSet> MaterialResourceDescSets;
        std::vector<std::shared_ptr<XJVulkanBuffer>> MaterialBuffers;
        std::vector<uint32_t> MaterialBufferSizes;


        bool IsValid() const
        {
            return FrameUboDescSetLayout &&
                MaterialParamDescSetLayout &&
                MaterialResourceDescSetLayout &&
                PipelineLayout &&
                Pipeline &&
                FrameDescriptorPool &&
                FrameUboDescSets[0] != VK_NULL_HANDLE &&
                FrameUboBuffers[0] &&
                ShaderLayout.HasPrimaryFrameUbo() &&
                ShaderLayout.HasPrimaryMaterialUbo();;
        }
        //材质是否拥有描述符
        bool HasMaterialDescriptors() const//材质是否拥有描述符
        {
            return MaterialDescriptorPool &&
                   !MaterialParamDescSets.empty() &&
                   !MaterialResourceDescSets.empty();
        }
        //清除材质描述符
        void ClearMaterialDescriptors()//清除材质描述符
        {
            MaterialParamDescSets.clear();
            MaterialResourceDescSets.clear();
            MaterialBuffers.clear();
            MaterialBufferSizes.clear();
            MaterialDescriptorPool.reset();
            LastDescriptorSetCount = 0;
        }


        void Clear()
        {
            ClearMaterialDescriptors();

            for (uint32_t frameSlot = 0; frameSlot < RENDERER_NUM_BUFFER; ++frameSlot)
            {
                FrameUboBuffers[frameSlot].reset();
                FrameUboDescSets[frameSlot] = VK_NULL_HANDLE;
            }
            FrameDescriptorPool.reset();

            Pipeline.reset();
            PipelineLayout.reset();

            MaterialResourceDescSetLayout.reset();
            MaterialParamDescSetLayout.reset();
            FrameUboDescSetLayout.reset();

            ShaderLayout = XJMaterialShaderRuntimeLayout{};
        }
    };


}

#endif
