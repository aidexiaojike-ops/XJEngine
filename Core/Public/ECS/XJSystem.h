#ifndef XJ_SYSTEM_H
#define XJ_SYSTEM_H


#include "Graphic/VulkanCommon.h"

namespace XJ
{

    class XJVulkanRenderPass;
    class XJRenderTarget;
    using XJVulkanCommandBuffer = VkCommandBuffer;

    class XJSystem
    {
        public:
            virtual void OnUpdate(float deltaTime){}
            
    };

    class XJMaterialSystem : public XJSystem
    {
        private:
            /* data */
        public:
            virtual void OnInit(XJVulkanRenderPass *renderPass) = 0;
            virtual void OnRender(XJVulkanCommandBuffer cmdBuffer, XJRenderTarget* renderTarget) = 0;
            virtual void OnDestroy() = 0;

    };
    class XJCameraSystem : public XJSystem
    {
        public:
            void OnUpdate(float deltaTime) override{};
    };
  
    
    

}

#endif