#ifndef XJ_EDITOR_TRANSFORM_TOOL_REQUESTS_H
#define XJ_EDITOR_TRANSFORM_TOOL_REQUESTS_H

#include "Tools/XJEditorTransformToolTypes.h"

#include <variant>

namespace XJ
{
    struct XJSetTransformSpace
    {
        XJEditorTransformSpace Space = XJEditorTransformSpace::Local;
    };

    using XJEditorTransformToolRequest = std::variant<XJSetTransformSpace>;
}

#endif