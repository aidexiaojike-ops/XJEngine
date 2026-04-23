#ifndef XJ_UNLIT_MATERIAL_SYSTEM_H
#define XJ_UNLIT_MATERIAL_SYSTEM_H

#include "ECS/System/XJMaterialSystem.h"
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"

namespace XJ
{
    
    class XJVulkanPipelineLayout;
    class XJVulkanPipeline;
    class XJVulkanDescriptorSetLayout;
    class XJVulkanDescriptorPool;

    class XJUnlitMaterialSystem : public XJMaterialSystem
    {
        public:
            virtual void OnInit(XJVulkanRenderPass *renderPass) override;
            virtual void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) override;
            virtual void OnDestroy() override;

            VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT;// 多重采样数量  minmap levels

        private:
            //动态创建材质集   动态扩容
            void ReCreateMaterialDescPool(uint32_t materialCount);
            //更新数据
            void UpdateFrameUboDescSet(XJRenderTarget *renderTarget);
            void UpdataMaterialParamsDescSet(VkDescriptorSet descSet, XJUnlitMaterial *material);
            void UpdataMaterialResourceDescSet(VkDescriptorSet descSet, XJUnlitMaterial *material);

            //shader 里面的三个输入
            std::shared_ptr<XJVulkanDescriptorSetLayout> mFrameUboDescSetLayout;
            std::shared_ptr<XJVulkanDescriptorSetLayout> mMaterialParamDescSetLayout;
            std::shared_ptr<XJVulkanDescriptorSetLayout> mMaterialResourceDescSetLayout;
            //渲染管线
            std::shared_ptr<XJVulkanPipelineLayout> mPipelineLayout;
            std::shared_ptr<XJVulkanPipeline> mPipeline;
            //描述符集
            std::shared_ptr<XJVulkanDescriptorPool> mDescriptorPool;
            std::shared_ptr<XJVulkanDescriptorPool> mMaterialDescriptorPool;

            VkDescriptorSet mFrameUboDescSet;
            std::shared_ptr<XJVulkanBuffer> mFrameUboBuffer;

            uint32_t mLastDescriptorSetCount = 0;//DescriptorSet 数量
            std::vector<VkDescriptorSet> mMaterialDescSets;//所有的材质描述符集
            std::vector<VkDescriptorSet> mMaterialResourceDescSets;//
            std::vector<std::shared_ptr<XJVulkanBuffer>> mMaterialBuffers;//所有材质参数


            std::shared_ptr<XJ::XJVulkanBuffer> mGlobalBuffer;
            std::shared_ptr<XJ::XJVulkanBuffer> mInstanceBuffer;
    };

}


#endif
