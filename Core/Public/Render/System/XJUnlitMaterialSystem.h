#ifndef XJ_UNLIT_MATERIAL_SYSTEM_H
#define XJ_UNLIT_MATERIAL_SYSTEM_H

#include "Render/System/XJMaterialRenderSystemBase.h"


namespace XJ
{
    class XJUnlitMaterialSystem : public XJMaterialRenderSystemBase
    {
        public:
            virtual void OnInit(XJVulkanRenderPass *renderPass) override;
            virtual void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) override;
            virtual void OnDestroy() override;

    };
}

#endif
