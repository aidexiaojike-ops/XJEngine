#ifndef XJ_SCENE_GIZMO_MATERIAL_SYSTEM_H
#define XJ_SCENE_GIZMO_MATERIAL_SYSTEM_H

#include "Render/System/XJMaterialSystem.h"

#include <memory>

namespace XJ
{
    class XJMesh;
    class XJVulkanPipeline;
    class XJVulkanPipelineLayout;

    // 只挂载到 Scene Preview，显示场景灯光和摄像机的线框。
    class XJSceneGizmoMaterialSystem final : public XJMaterialSystem
    {
        public:
            void OnInit(XJVulkanRenderPass* renderPass) override;
            void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) override;
            void OnDestroy() override;

        private:
            std::shared_ptr<XJMesh> mDirectionalMesh;
            std::shared_ptr<XJMesh> mPointMesh;
            std::shared_ptr<XJMesh> mSpotMesh;
            std::shared_ptr<XJMesh> mCameraMesh;
            std::shared_ptr<XJVulkanPipelineLayout> mPipelineLayout;
            std::shared_ptr<XJVulkanPipeline> mPipeline;
    };
}

#endif
