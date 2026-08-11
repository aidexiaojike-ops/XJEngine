#ifndef XJ_VIEWPORT_H
#define XJ_VIEWPORT_H

#include <imgui.h>
#include <cstdint>

namespace XJ
{
    struct ViewportSettings
    {
        uint32_t mWidth = 64;
        uint32_t mHeight = 64;

        const char* mViewportName = "Viewport";
    };

    class XJViewport
    {
        public:

            virtual ~XJViewport() = default;

        public:

            virtual void Resize(uint32_t width, uint32_t height);
            virtual void DrawUI();

            void SetViewportName(const char* name) { mSettings.mViewportName = name; }

        protected:
            virtual ImTextureID GetViewportTextureID() const = 0;
            virtual bool IsViewportTextureReady() const = 0;
            virtual void OnViewportResized(uint32_t width, uint32_t height) = 0;

        protected:
            bool mOpen = true;
            bool mHovered = false;
            bool mFocused = false;
            ViewportSettings mSettings;
    };
}

#endif
