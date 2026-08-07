#ifndef XJ_UNLIT_MATERIAL_SYSTEM_H
#define XJ_UNLIT_MATERIAL_SYSTEM_H

#include "Render/System/XJMaterialRenderSystemBase.h"
#include "Render/Material/XJMaterialRenderItem.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace XJ
{
   struct XJMaterialPipelineRuntime;

    class XJUnlitMaterialSystem : public XJMaterialRenderSystemBase
    {
        public:
            virtual void OnInit(XJVulkanRenderPass* renderPass) override;
            virtual void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) override;
            virtual void OnDestroy() override;

        private:
            void MarkMaterialParamsDirtyForAllFrameSlots(
                XJMaterialPipelineRuntime& runtime,
                uint32_t materialIndex);

            void MarkMaterialResourcesDirtyForAllFrameSlots(
                XJMaterialPipelineRuntime& runtime,
                uint32_t materialIndex);

            bool HasPendingMaterialParamUpdates(
                XJMaterialPipelineRuntime& runtime,
                uint32_t materialIndex) const;

            bool HasPendingMaterialResourceUpdates(
                XJMaterialPipelineRuntime& runtime,
                uint32_t materialIndex) const;

        private:
            std::vector<XJMaterialRenderItem> mRenderItems;

            std::unordered_set<XJMaterialPipelineRuntime*> mForceUpdateRuntimes;// 需要强制更新材质描述符的 runtime
            std::unordered_set<XJMaterialPipelineRuntime*> mUpdatedFrameRuntimes;// 记录本帧已经更新过材质描述符的 runtime，避免重复更新
            std::unordered_map<XJMaterialPipelineRuntime*, uint32_t> mRequiredDescriptorCountByRuntime;// 记录每个 runtime 当前帧需要的材质描述符数量，确保 descriptor pool 足够大

            // true 表示该 frame-slot/material descriptor 已经和 CPU 材质数据同步。
            // dirty 时把所有 frame slot 对应项设回 false，然后逐帧在安全 frame slot 上更新。
            std::unordered_map<XJMaterialPipelineRuntime*, std::vector<bool>> mParamUploadedByRuntime;
            std::unordered_map<XJMaterialPipelineRuntime*, std::vector<bool>> mResourceUploadedByRuntime;// 记录每个 runtime 的材质资源描述符是否已经上传到 GPU，避免重复上传
    };
}

#endif
