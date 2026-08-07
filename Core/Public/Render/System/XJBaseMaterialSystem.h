#ifndef XJ_BASEMATERIALSYSTEM_H
#define XJ_BASEMATERIALSYSTEM_H

#include "Render/System/XJMaterialSystem.h"
#include "ECS/Component/Material/XJBaseMaterialComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "Render/XJRenderer.h"
#include <array>
#include <cstddef>

namespace XJ
{
    class XJVulkanPipelineLayout;
    class XJVulkanPipeline;
    class XJVulkanDescriptorSetLayout;
    class XJVulkanDescriptorPool;
    class XJVulkanBuffer;
    class XJTexture;
    class XJSampler;

    struct GlobalUbo
    {
        /* data */
        glm::mat4 projMat{1.0f};
        glm::mat4 viewMat{1.0f};
    };

    // 与 Shader/Descriptor.vert 的 `layout(set=0, binding=0, std140) uniform GlobalUbo` 一致。
    static_assert(sizeof(GlobalUbo) == 128,            "GlobalUbo: sizeof 必须为 128（std140：2 * mat4）");
    static_assert(offsetof(GlobalUbo, projMat) == 0,   "GlobalUbo.projMat 偏移必须为 0");
    static_assert(offsetof(GlobalUbo, viewMat) == 64,  "GlobalUbo.viewMat 偏移必须为 64");

    struct InstanceUbo
    {
        glm::mat4 modelMat{1.0f};
    };

    // 与 Shader/Descriptor.vert 的 `layout(set=0, binding=1, std140) uniform InstanceUbo` 一致。
    static_assert(sizeof(InstanceUbo) == 64,                "InstanceUbo: sizeof 必须为 64（std140：1 * mat4）");
    static_assert(offsetof(InstanceUbo, modelMat) == 0,     "InstanceUbo.modelMat 偏移必须为 0");

    class XJBaseMaterialSystem : public XJMaterialSystem
    {
        private:
            /* data */
            std::shared_ptr<XJVulkanPipelineLayout>         mPipelineLayout;
            std::shared_ptr<XJVulkanPipeline>               mPipeline;
            std::shared_ptr<XJVulkanDescriptorSetLayout>    mDescriptorSetLayout;

            VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT;// 多重采样数量  minmap levels
            static constexpr uint32_t MAX_ENTITIES  = 10000; // 最大实体数量
            uint32_t mDynamicAlignment  = 0; // 当前实体数量对应的动态对齐大小
        public:

            void OnInit(XJVulkanRenderPass *renderPass) override;
            void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget *renderTarget) override;
            void OnDestroy() override;

            void UpdateDescriptorSets();


           // 每个 in-flight 帧槽位一份 CPU/GPU UBO，避免 CPU 写当前帧时覆盖 GPU 仍在读的上一帧数据。
            std::array<GlobalUbo, RENDERER_NUM_BUFFER> mGlobalUbo{};
            std::array<InstanceUbo, RENDERER_NUM_BUFFER> mInstanceUbo{};
           std::array<std::shared_ptr<XJ::XJVulkanBuffer>, RENDERER_NUM_BUFFER> mGlobalBuffers;
            std::array<std::shared_ptr<XJ::XJVulkanBuffer>, RENDERER_NUM_BUFFER> mInstanceBuffers;

            std::shared_ptr<XJ::XJTexture> mTextureA;
            std::shared_ptr<XJ::XJTexture> mTextureB;
            std::shared_ptr<XJ::XJSampler> mSamplerA;
            std::shared_ptr<XJ::XJSampler> mSamplerB;

            // 每个帧槽位一个 descriptor set，分别绑定该槽位自己的 UBO buffer。
            std::vector<VkDescriptorSet> mDescriptorSets;
            std::shared_ptr<XJVulkanDescriptorPool>  mDescriptorPool;
            
    };
    
 

}

#endif
