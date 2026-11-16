#ifndef XJ_SELECTION_OUTLINE_MATERIAL_SYSTEM_H
#define XJ_SELECTION_OUTLINE_MATERIAL_SYSTEM_H

#include "Render/System/XJMaterialSystem.h"

#include <memory>
#include <unordered_map>

namespace XJ
{
    class XJMesh;
    class XJVulkanPipeline;
    class XJVulkanPipelineLayout;

    struct XJEditorUIState;

    // 只挂载到 Scene Preview。
    // 对当前选中实体的 Mesh 绘制编辑器轮廓。
    class XJSelectionOutlineMaterialSystem final : public XJMaterialSystem
    {
        public:
            explicit XJSelectionOutlineMaterialSystem( XJEditorUIState* state);

            void OnInit(XJVulkanRenderPass* renderPass) override;

            void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) override;

            void OnDestroy() override;

        private:
            struct OutlineMeshEntry
            {
                std::weak_ptr<XJMesh> Source;
                std::shared_ptr<XJMesh> Outline;
            };

            std::shared_ptr<XJMesh> GetOutlineMesh(const std::shared_ptr<XJMesh>& source);

            void PruneCache();

            // 非拥有指针。
            // 与现有 ScenePreview 的 UIState 绑定遵循相同生命周期。
            XJEditorUIState* mState = nullptr;

            std::shared_ptr<XJVulkanPipelineLayout> mPipelineLayout;

            // 正向缩放：剔除正面，绘制背面壳。
            std::shared_ptr<XJVulkanPipeline> mPipelineCullFront;

            // 负缩放会翻转三角形绕序，需要反向剔除。
            std::shared_ptr<XJVulkanPipeline> mPipelineCullBack;

            std::unordered_map<const XJMesh*, OutlineMeshEntry> mCache;
    };
}

#endif