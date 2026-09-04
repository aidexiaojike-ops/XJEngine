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
            virtual ~XJSystem() = default;

            virtual void OnCreate() {}
            virtual void OnUpdate(float deltaTime){}
            virtual void OnFixedUpdate(float fixedDeltaTime) {}
            virtual void OnDestroy() {}
    };
}

#endif