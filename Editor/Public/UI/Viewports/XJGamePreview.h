#ifndef XJ_GAME_PREVIEW_H
#define XJ_GAME_PREVIEW_H


#include "UI/Viewports/XJViewport.h"


namespace XJ
{
    class XJEntity;

    class XJGamePreview : public XJViewport
    {
        public:

            virtual bool Render(VkCommandBuffer cmd) override;//游戏窗口渲染
            

             void SetCamera(XJEntity* camera)
             {
                 mGameCamera  = camera;
             }

        private:

            XJEntity* mGameCamera  = nullptr;
            
        protected:
            virtual void CreateRenderPass(VulkanPhysicalDevices* physicalDevices) override;
    };
    

}

#endif