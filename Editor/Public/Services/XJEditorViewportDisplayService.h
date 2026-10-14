#ifndef XJ_EDITOR_VIEWPORT_DISPLAY_SERVICE_H
#define XJ_EDITOR_VIEWPORT_DISPLAY_SERVICE_H

#include "Diagnostics/XJPerformanceData.h"
#include "Render/XJRenderViewOptions.h"
#include "Viewport/XJEditorViewportTypes.h"

#include <memory>
//
namespace XJ
{
    struct XJEditorViewportCapabilities//编辑器视口能力
    {
        bool Available = false;
        uint64_t BindingEpoch = 0;//用于跟踪绑定的生命周期 
        XJRenderViewCapabilities  Renderer;
    };

    class IXJEditorViewportDisplayService//可注入的 DisplayService
    {
        public:
            virtual ~IXJEditorViewportDisplayService() = default;

            virtual void BeginStatisticsFrame(uint64_t frameId) = 0;
            virtual XJRenderStatistics ReadStatistics(XJEditorViewportId viewport) const = 0;
            virtual XJEditorViewportCapabilities GetCapabilities(XJEditorViewportId viewport) const = 0;
            virtual bool ApplyViewOptions(XJEditorViewportId viewport, const XJRenderViewOptions& options) = 0;

    };

    class XJEditorViewportSystem;

    class XJEditorViewportDisplayService final : public IXJEditorViewportDisplayService
    {
        public:
            void Bind(XJEditorViewportSystem& host);
            void Unbind();

            void BeginStatisticsFrame(uint64_t frameId) override;

            XJRenderStatistics ReadStatistics(XJEditorViewportId viewport) const override;

            XJEditorViewportCapabilities GetCapabilities(XJEditorViewportId viewport) const override;

            bool ApplyViewOptions(
                XJEditorViewportId viewport,
                const XJRenderViewOptions& options) override;

        private:
            bool HasHost() const;

            XJEditorViewportSystem* mHost = nullptr;
            std::weak_ptr<void> mLifetime;
            uint64_t mBindingEpoch = 0;
    };
}

#endif