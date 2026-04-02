#ifndef XJ_BASEMATERIALSYSTEM_H
#define XJ_BASEMATERIALSYSTEM_H

#include "ECS/XJSystem.h"
#include "ECS/Component/XJBaseMaterialComponent.h"
#include "ECS/Component/XJMeshComponent.h"
#include "ECS/Component/XJTransformComponent.h"

#include "Graphic/XJVulkanDescriptorSet.h"  // 解决 XJVulkanDescriptorPool 定义
#include "Render/XJTexture.h"


namespace XJ
{
    class XJVulkanPipelineLayout;
    class XJVulkanPipeline;
    class XJVulkanDescriptorSetLayout;
    class VulkanImageView;

    struct GlobalUbo
    {
        /* data */
        glm::mat4 projMat{1.0f};
        glm::mat4 viewMat{1.0f};
    };

    struct InstanceUbo
    {
        glm::mat4 modelMat{1.0f};
    };

    class XJBaseMaterialSystem : public XJMaterialSystem
    {
        private:
            /* data */
            std::shared_ptr<XJVulkanPipelineLayout>         mPipelineLayout;
            std::shared_ptr<XJVulkanPipeline>               mPipeline;
            std::shared_ptr<XJVulkanDescriptorSetLayout>    mDescriptorSetLayout;

            VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT;// 多重采样数量  minmap levels
            static constexpr uint32_t MAX_ENTITIES  = 16; // 最大实体数量
            uint32_t mDynamicAlignment  = 0; // 当前实体数量对应的动态对齐大小
        public:

            void OnInit(XJVulkanRenderPass *renderPass) override;
            void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget *renderTarget) override;
            void OnDestroy() override;

            void UpdateDescriptorSets(VkCommandBuffer cmdBuffer);


            GlobalUbo mGlobalUbo;
            InstanceUbo mInstanceUbo;
            std::shared_ptr<XJ::XJVulkanBuffer> mGlobalBuffer;
            std::shared_ptr<XJ::XJVulkanBuffer> mInstanceBuffer;
            std::shared_ptr<XJ::XJTexture> mTextureA;
            std::shared_ptr<XJ::XJTexture> mTextureB;

            std::vector<VkDescriptorSet>                        mDescriptorSets;
            std::shared_ptr<XJVulkanDescriptorPool>  mDescriptorPool;
            
    };
    
 

}

#endif