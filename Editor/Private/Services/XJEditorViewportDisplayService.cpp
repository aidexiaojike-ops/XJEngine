#include "Services/XJEditorViewportDisplayService.h"
#include "UI/Viewports/XJEditorViewportSystem.h"

namespace XJ
{
    void XJEditorViewportDisplayService::Bind(XJEditorViewportSystem& host)//绑定
    {
        mHost = &host;
        mLifetime = host.GetLifetimeToken();
        ++mBindingEpoch;
    }

    void XJEditorViewportDisplayService::Unbind()//解绑
    {
        mHost = nullptr;
        mLifetime.reset();
        ++mBindingEpoch;
    }

    bool XJEditorViewportDisplayService::HasHost() const//是否有宿主
    {
        return mHost && !mLifetime.expired();
    }

    void XJEditorViewportDisplayService::BeginStatisticsFrame(uint64_t frameId)//开始统计帧
    {
        if (HasHost())
            mHost->BeginStatisticsFrame(frameId);
    }

    XJRenderStatistics XJEditorViewportDisplayService::ReadStatistics(XJEditorViewportId viewport) const
    {
        return HasHost() ? mHost->ReadStatistics(viewport)
                         : XJRenderStatistics{};
    }

    XJEditorViewportCapabilities XJEditorViewportDisplayService::GetCapabilities(XJEditorViewportId viewport) const
    {
        XJEditorViewportCapabilities result;
        result.BindingEpoch = mBindingEpoch;

        if (HasHost())
        {
            result.Available = mHost->IsRenderViewAvailable(viewport);
            result.Renderer = mHost->ReadViewCapabilities(viewport);
        }

        return result;
    }

    bool XJEditorViewportDisplayService::ApplyViewOptions(
        XJEditorViewportId viewport,
        const XJRenderViewOptions& options)
    {
        return HasHost() && mHost->ApplyViewOptions(viewport, options);
    }
}