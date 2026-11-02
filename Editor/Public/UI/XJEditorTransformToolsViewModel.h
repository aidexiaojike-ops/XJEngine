#ifndef XJ_EDITOR_TRANSFORM_TOOLS_VIEW_MODEL_H
#define XJ_EDITOR_TRANSFORM_TOOLS_VIEW_MODEL_H

#include "Tools/XJEditorTransformToolTypes.h"

#include <cstdint>

namespace XJ
{
    struct XJEditorTransformToolsViewModel
    {
        XJEditorTransformSpace Space = XJEditorTransformSpace::Local;

        // 当前是否允许改变工具坐标空间。
        // 第一版仅在有效编辑场景的 Edit 模式下允许。
        bool CanChangeSpace = false;

        // 仅在坐标空间实际改变时递增。
        // 后续拖动会记录此版本，检测工具设置是否变化。
        uint64_t SettingsRevision = 0;
    };
}

#endif