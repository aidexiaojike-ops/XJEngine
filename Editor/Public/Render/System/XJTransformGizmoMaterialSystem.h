#ifndef XJ_TRANSFORM_GIZMO_MATERIAL_SYSTEM_H
#define XJ_TRANSFORM_GIZMO_MATERIAL_SYSTEM_H

#include "Render/System/XJMaterialSystem.h"
#include "Tools/XJTransformGizmoGeometry.h"

#include <memory>
#include <vector>

namespace XJ
{
    class XJMesh;
    class XJVulkanPipeline;
    class XJVulkanPipelineLayout;

    struct XJEditorUIState;

    class XJTransformGizmoMaterialSystem final
        : public XJMaterialSystem
    {
        public:
            explicit XJTransformGizmoMaterialSystem(XJEditorUIState* state);

            void OnInit(XJVulkanRenderPass* renderPass) override;

            void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) override;

            void OnDestroy() override;

        private:
            struct Part
            {
                XJTransformGizmoHandle Handle = XJTransformGizmoHandle::None;
                std::shared_ptr<XJMesh> Mesh;
            };

            // 非拥有指针。
            // 与现有 ScenePreview 的 UIState 绑定遵循相同生命周期。
            XJEditorUIState* mState = nullptr;
            std::vector<Part> mParts; 
            std::shared_ptr<XJVulkanPipelineLayout> mPipelineLayout;
            std::shared_ptr<XJVulkanPipeline> mPipeline;
    };
}

#endif