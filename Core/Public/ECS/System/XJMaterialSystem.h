#ifndef XJ_MATERIAL_SYSTEM_H
#define XJ_MATERIAL_SYSTEM_H

#include "Graphic/VulkanCommon.h"
#include "ECS/XJSystem.h"
#include "glm/glm.hpp"

namespace XJ
{
    class XJVulkanRenderPass;
    class XJRenderTarget;
    class XJApplication;
    class XJScene;
    class XJVulkanDevice;
    class XJEntity;  // 添加这行
    class XJCameraComponent;  // 添加这行
    using XJVulkanCommandBuffer = VkCommandBuffer;

    class XJMaterialSystem : public XJSystem
    {
        private:
            /* data */
        public:
            virtual void OnInit(XJVulkanRenderPass *renderPass) = 0;
            virtual void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) = 0;
            virtual void OnDestroy() = 0;

        protected:
            XJApplication *XJGetApp() const;
            XJScene *XJGetScene() const;
            XJVulkanDevice *XJGetDevice() const;
            const glm::mat4 XJGetProjMat(XJRenderTarget *renderTarget) const;
            const glm::mat4 XJGetViewMat(XJRenderTarget *renderTarget) const;

    };
}

#endif