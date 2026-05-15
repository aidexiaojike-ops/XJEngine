#ifndef XJ_SCENE_PREVIEW_H
#define XJ_SCENE_PREVIEW_H

#include "UI/Viewports/XJViewport.h"

namespace XJ
{
     class XJEntity;

    class XJScenePreview : public XJViewport
    {
        public:

            virtual bool Render(VkCommandBuffer cmd) override;

            void SetCamera(XJEntity* camera)
            {
                mPreviewCamera = camera;
            }

        private:

            XJEntity* mPreviewCamera = nullptr;

        protected:
            virtual void CreateRenderPass(VulkanPhysicalDevices* physicalDevices) override;
    };
}

#endif