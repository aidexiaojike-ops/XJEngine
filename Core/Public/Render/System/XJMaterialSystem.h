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

            // 注入渲染场景：ScenePreview 用编辑器场景，GamePreview 在 Play 时用运行时克隆。
            void SetScene(XJScene* scene){mScene = scene;}

        protected:
            XJApplication *XJGetApp() const;
            XJScene *XJGetScene() const;
            XJVulkanDevice *XJGetDevice() const;
            const glm::mat4 XJGetProjMat(XJRenderTarget *renderTarget) const;
            const glm::mat4 XJGetViewMat(XJRenderTarget *renderTarget) const;

            XJScene* mScene = nullptr;   // 注入的场景；为空时回退全局场景

    };
}

#endif
