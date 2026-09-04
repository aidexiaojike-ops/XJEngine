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

    // ★★★ XJ_MARKER_PLAYMODE_CONTROLS_20260903 ★★★
    void XJGamePreview::DrawPlayControls()
    {
        const bool playing = (mPlayState == XJEditorPlayState::Playing);
        const bool paused  = (mPlayState == XJEditorPlayState::Paused);

        auto request = [this](XJEditorPlayState state)
        {
            // 乐观更新显示，Runtime 处理失败时会再纠正。
            mPlayState = state;
            if (mPlayStateChangeCallback)
                mPlayStateChangeCallback(state);
        };
        /*
        if(playing)
        {
            if (ImGui::Button("Pause"))
                request(XJEditorPlayState::Paused);
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
                request(XJEditorPlayState::Edit);
        }
        else if(paused)
        {
            if (ImGui::Button("Resume"))
                request(XJEditorPlayState::Playing);
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
                request(XJEditorPlayState::Edit);
        }
        else
        {
            if (ImGui::Button("Play"))
                request(XJEditorPlayState::Playing);
        }
        */
        if (playing)
        {
            if (ImGui::Button("Pause"))
                request(XJEditorPlayState::Paused);
        }
        else if (paused)
        {
            if (ImGui::Button("Resume"))
                request(XJEditorPlayState::Playing);
        }
        else
        {
            if (ImGui::Button("Play"))
                request(XJEditorPlayState::Playing);
        }

        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            request(XJEditorPlayState::Edit);
    }
    void XJGamePreview::DrawUI()
    {
        mHovered = false;
        mFocused = false;

        if(!mOpen)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::Begin(mSettings.mViewportName, &mOpen))
        {
            mFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            // 窗口顶部 Play/Pause/Stop 控制条
            DrawPlayControls();
            ImGui::Separator();

            ImVec2 avail = ImGui::GetContentRegionAvail();
            if (avail.x > 1.0f && avail.y > 1.0f)
            {
                Resize(static_cast<uint32_t>(avail.x), static_cast<uint32_t>(avail.y));
                if (IsViewportTextureReady())
                {
                    ImGui::Image(GetViewportTextureID(), avail);
                    mHovered = ImGui::IsItemHovered();
                }
                else
                {
                    ImGui::TextUnformatted("Preview Texture Invalid.");
                }
            }
            else
            {
                ImGui::TextUnformatted("Preview window too small.");
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }
}
