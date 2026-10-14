#ifndef XJ_EDITOR_VIEWPORT_TYPES_H
#define XJ_EDITOR_VIEWPORT_TYPES_H

#include <cstddef>
//公共视口标识
namespace XJ
{
    enum class XJEditorViewportId : size_t
    {
        Scene,
        Game,
        Count
    };

    inline constexpr size_t XJ_EDITOR_VIEWPORT_COUNT = static_cast<size_t>(XJEditorViewportId::Count);

    enum class XJEditorRenderMode
    {
        Final,
        BaseColor,
        LinearDepth,
        WorldNormal,
        Lighting
    };
}

#endif