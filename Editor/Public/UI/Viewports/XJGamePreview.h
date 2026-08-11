#ifndef XJ_GAME_PREVIEW_H
#define XJ_GAME_PREVIEW_H


#include "UI/Viewports/XJViewport.h"
#include "UI/Viewports/XJViewportRenderSurface.h"


namespace XJ
{
    class XJEntity;

    class XJGamePreview : public XJViewport
    {
        public:

            bool Init(XJRenderContext* renderContext);
            void Shutdown();
            void PrepareBeforeRender();
            void PostRender();
            bool Render(VkCommandBuffer cmd);//游戏窗口渲染

            template<typename T, typename... Args>
            void AddMaterialSystem(Args&&... args)
            {
                mSurface.template AddMaterialSystem<T>(std::forward<Args>(args)...);
            }
            

             void SetCamera(XJEntity* camera)
             {
                 mGameCamera  = camera;
             }

        private:

            XJViewportRenderSurface mSurface;
            XJEntity* mGameCamera  = nullptr;
             
        protected:
            ImTextureID GetViewportTextureID() const override;
            bool IsViewportTextureReady() const override;
            void OnViewportResized(uint32_t width, uint32_t height) override;
    };
    

}

#endif
