#ifndef XJ_RENDER_VIEW_OPTIONS_H
#define XJ_RENDER_VIEW_OPTIONS_H

#include <array>
#include <cstddef>
#include <cstdint>
//渲染配置和能力数据
namespace XJ
{
    enum class XJRenderViewMode  : uint32_t//渲染视图选项
    {
        Final,
        BaseColor,
        LinearDepth,
        WorldNormal,
        Lighting,
        Count
    };

    inline constexpr size_t XJ_RENDER_VIEW_MODE_COUNT = static_cast<size_t>(XJRenderViewMode ::Count);//渲染视图模式数量

    struct  XJRenderViewOptions//渲染视图选项
    {
        /* data */
        XJRenderViewMode Mode = XJRenderViewMode::Final;
        float DepthRange = 10.0f;
    };

    struct XJRenderViewCapabilities//渲染视图能力
    {
        uint64_t Revision = 1;

        // 当前只实现最终画面。
        std::array<bool, XJ_RENDER_VIEW_MODE_COUNT> Supported{
            true, false, false, false, false
        };

        bool Supports(XJRenderViewMode mode) const
        {
            const size_t index = static_cast<size_t>(mode);
            return index < Supported.size() && Supported[index];
        }
    };
}

#endif