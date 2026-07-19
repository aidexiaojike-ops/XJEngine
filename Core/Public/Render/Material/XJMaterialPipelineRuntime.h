#ifndef XJ_MATERIAL_PIPELINE_RUNTIME_H
#define XJ_MATERIAL_PIPELINE_RUNTIME_H

#include "Render/Material/XJMaterialShaderRuntimeLayout.h"

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
        VkDescriptorSet FrameUboDescSet = VK_NULL_HANDLE;
        std::shared_ptr<XJVulkanBuffer> FrameUboBuffer;

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
                FrameUboDescSet != VK_NULL_HANDLE &&
                FrameUboBuffer &&
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

            FrameUboBuffer.reset();
            FrameUboDescSet = VK_NULL_HANDLE;
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