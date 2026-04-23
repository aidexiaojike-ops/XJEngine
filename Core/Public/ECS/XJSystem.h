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

    class XJCameraSystem : public XJSystem
    {
        public:
            void OnUpdate(float deltaTime) override{};
    };
  
    
    

}

#endif