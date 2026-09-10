#ifndef XJ_LIGHT_GIZMO_MATERIAL_SYSTEM_H
#define XJ_LIGHT_GIZMO_MATERIAL_SYSTEM_H

#include "Render/System/XJMaterialSystem.h"

#include <memory>

namespace XJ
{
    class XJMesh;
    class XJVulkanPipeline;
    class XJVulkanPipelineLayout;

    // 只挂载到 Scene Preview，显示 LightComponent 的编辑器线框模型。
    class XJLightGizmoMaterialSystem final : public XJMaterialSystem
    {
        public:
            void OnInit(XJVulkanRenderPass* renderPass) override;
            void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) override;
            void OnDestroy() override;

        private:
            std::shared_ptr<XJMesh> mDirectionalMesh;
            std::shared_ptr<XJMesh> mPointMesh;
            std::shared_ptr<XJMesh> mSpotMesh;
            std::shared_ptr<XJVulkanPipelineLayout> mPipelineLayout;
            std::shared_ptr<XJVulkanPipeline> mPipeline;
    };
}

#endif
