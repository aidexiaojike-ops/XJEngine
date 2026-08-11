#include "UI/Viewports/XJViewport.h"

namespace XJ
{
    void XJViewport::Resize(uint32_t width, uint32_t height)
    {
        if (width < 64 || height < 64)
            return;

        if (width == mSettings.mWidth && height == mSettings.mHeight)
            return;

        mSettings.mWidth = width;
        mSettings.mHeight = height;
        OnViewportResized(width, height);
    }


    void XJViewport::DrawUI()
    {
        mHovered = false;
        mFocused = false;

        if (!mOpen)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        if (ImGui::Begin(mSettings.mViewportName, &mOpen))
        {
            mFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

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
