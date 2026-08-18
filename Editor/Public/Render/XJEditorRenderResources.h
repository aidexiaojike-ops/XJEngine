#ifndef XJ_EDITOR_RENDER_RESOURCES_H
#define XJ_EDITOR_RENDER_RESOURCES_H

#include <memory>

namespace XJ
{
    class XJRenderContext;
    class XJTexture;
    class XJSampler;

    class XJEditorRenderResources
    {
        public:
            XJEditorRenderResources();
            ~XJEditorRenderResources();

            XJEditorRenderResources(const XJEditorRenderResources&) = delete;
            XJEditorRenderResources& operator=(const XJEditorRenderResources&) = delete;

            bool Init(XJRenderContext& renderContext);
            void Shutdown();

            const std::shared_ptr<XJTexture>& GetDefaultTexture() const;

            const std::shared_ptr<XJSampler>& GetDefaultSampler() const;

            bool IsInitialized() const;

        private:
            XJRenderContext* mRenderContext = nullptr;

            std::shared_ptr<XJTexture> mDefaultTexture;
            std::shared_ptr<XJSampler> mDefaultSampler;

            bool mInitialized = false;
    };
}

#endif