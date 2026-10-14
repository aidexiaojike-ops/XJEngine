#include "UI/Viewports/XJGamePreview.h"
#include "UI/XJEditorUIState.h"
#include "UI/XJViewportDisplayWidgets.h"

#include "ECS/XJEntity.h"

#include <algorithm>

namespace XJ
{
    bool XJGamePreview::Init(XJRenderContext* renderContext)
    {
        return mSurface.Init(renderContext, mSettings.mWidth, mSettings.mHeight, true);
    }

    void XJGamePreview::Shutdown()
    {
        mUIState = nullptr;
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
            // 只发送请求；实际状态由 Runtime/PlayController 成功处理后回写。
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

        if (!mOpen)
            return;

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        if (ImGui::Begin(mSettings.mViewportName, &mOpen))
        {
            mFocused = ImGui::IsWindowFocused(
                ImGuiFocusedFlags_RootAndChildWindows);

            // 左侧播放控制。
            DrawPlayControls();

            const float controlsRight =
                ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x;

            if (mUIState)
            {
                const float menuX =
                    ImGui::GetWindowContentRegionMax().x - 112.0f;

                if (menuX > controlsRight + ImGui::GetStyle().ItemSpacing.x)
                    ImGui::SameLine();

                // 窄窗口换行后仍右对齐。
                ImGui::SetCursorPosX(std::max(
                    ImGui::GetWindowContentRegionMin().x, menuX));

                XJViewportDisplayWidgets::DrawMenu(
                    *mUIState, XJEditorViewportId::Game);
            }

            ImGui::Separator();

            const ImVec2 available = ImGui::GetContentRegionAvail();

            if (available.x > 1.0f && available.y > 1.0f)
            {
                Resize(
                    static_cast<uint32_t>(available.x),
                    static_cast<uint32_t>(available.y));

                if (IsViewportTextureReady())
                {
                    const ImVec2 imagePosition = ImGui::GetCursorScreenPos();

                    ImGui::Image(GetViewportTextureID(), available);
                    mHovered = ImGui::IsItemHovered();

                    if (mUIState)
                    {
                        XJViewportDisplayWidgets::DrawPerformance(
                            *mUIState,
                            XJEditorViewportId::Game,
                            imagePosition,
                            available);
                    }
                }
                else
                {
                    ImGui::TextUnformatted("Preview Texture Invalid.");
                }
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }
}
