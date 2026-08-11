#include "UI/Viewports/XJGamePreview.h"

#include "ECS/XJEntity.h"

namespace XJ
{
    bool XJGamePreview::Init(XJRenderContext* renderContext)
    {
        return mSurface.Init(renderContext, mSettings.mWidth, mSettings.mHeight, true);
    }

    void XJGamePreview::Shutdown()
    {
        mSurface.Shutdown();
    }

    void XJGamePreview::PrepareBeforeRender()
    {
        mSurface.PrepareBeforeRender();
    }

    void XJGamePreview::PostRender()
    {
        mSurface.PostRender();
    }

    bool XJGamePreview::Render(VkCommandBuffer cmd)
    {
        mSurface.SetCamera(mGameCamera);

        if (!mSurface.BeginRender(cmd))
            return false;

        if (mGameCamera)
            mSurface.RenderMaterialSystem(cmd);

        mSurface.EndRender(cmd);
        return true;
    }

    ImTextureID XJGamePreview::GetViewportTextureID() const
    {
        return mSurface.GetTextureID();
    }

    bool XJGamePreview::IsViewportTextureReady() const
    {
        return mSurface.IsTextureReady();
    }

    void XJGamePreview::OnViewportResized(uint32_t width, uint32_t height)
    {
        mSurface.Resize(width, height);
    }
}
