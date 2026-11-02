#ifndef XJ_WORLD_GRID_MATERIAL_SYSTEM_H
#define XJ_WORLD_GRID_MATERIAL_SYSTEM_H

#include "Render/System/XJMaterialSystem.h"

#include <memory>

namespace XJ
{
    class XJVulkanPipeline;
    class XJVulkanPipelineLayout;
    // 只挂载到 Scene Preview，显示世界网格。
    class XJWorldGridMaterialSystem final : public XJMaterialSystem
    {
        public:
            void OnInit(
                XJVulkanRenderPass* renderPass) override;

            void OnRender(
                XJVulkanCommandBuffer cmdBuffer,
                XJRenderTarget* renderTarget) override;

            void OnDestroy() override;

        private:
            std::shared_ptr<XJVulkanPipelineLayout>
                mPipelineLayout;

            std::shared_ptr<XJVulkanPipeline>
                mPipeline;
    };
}

#endif