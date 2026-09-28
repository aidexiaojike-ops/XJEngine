#ifndef XJ_SURFACE_MATERIAL_SYSTEM_H
#define XJ_SURFACE_MATERIAL_SYSTEM_H

#include "Render/System/XJMaterialRenderSystemBase.h"
#include "Render/Material/XJMaterialRenderItem.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace XJ
{
   struct XJMaterialPipelineRuntime;

    class XJSurfaceMaterialSystem : public XJMaterialRenderSystemBase
    {
        public:
            virtual void OnInit(XJVulkanRenderPass* renderPass) override;
            virtual void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) override;
            virtual void OnDestroy() override;

        private:
            std::vector<XJMaterialRenderItem> mRenderItems;

            std::unordered_set<XJMaterialPipelineRuntime*> mUpdatedFrameRuntimes;// 记录本帧已经更新过材质描述符的 runtime，避免重复更新
            std::unordered_map<XJMaterialPipelineRuntime*, uint32_t> mRequiredDescriptorCountByRuntime;// 记录每个 runtime 当前帧需要的材质描述符数量，确保 descriptor pool 足够大

            struct XJMaterialUploadStamp
            {
                uint64_t OwnerId = 0;
                uint64_t ParameterRevision = 0;
                uint64_t ResourceRevision = 0;
            };

            struct XJRuntimeUploadState
            {
                std::vector<XJMaterialUploadStamp> Slots;
            };

            std::unordered_map<XJMaterialPipelineRuntime*, XJRuntimeUploadState> mUploadStates;
    };
}

#endif
