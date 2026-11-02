#ifndef XJ_EDITOR_TRANSFORM_TOOL_TYPES_H
#define XJ_EDITOR_TRANSFORM_TOOL_TYPES_H

#include <cstdint>


namespace XJ
{
    // 编辑器工具使用的操作坐标空间。
    // 不依赖脚本层的 XJScriptSpace。
    enum class XJEditorTransformSpace : uint8_t
    {
        Local,
        World
    };

    constexpr  bool IsValidTransformSpace(XJEditorTransformSpace space)
    {
        switch (space)
        {
            case XJEditorTransformSpace::Local:
            case XJEditorTransformSpace::World:
            return true;
        }
        return false;
    }
}

#endif