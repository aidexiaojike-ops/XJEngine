#ifndef XJ_EDITOR_TRANSFORM_TOOLS_CONTROLLER_H
#define XJ_EDITOR_TRANSFORM_TOOLS_CONTROLLER_H

#include "Tools/XJEditorTransformToolTypes.h"

#include <cstdint>

namespace XJ
{
    struct  XJEditorUIState;
    class XJScene;

    class XJEditorTransformToolsController
    {
        public:
            void ProcessRequests(XJEditorUIState& state, bool canChangeSpace);
            // Workspace 完整关闭时重置。
            // 普通切换场景时保留工具偏好。
            void Reset(XJEditorUIState& state);
            void RefreshGizmo(XJEditorUIState& state, XJScene* scene, bool canEdit);//刷新变换控件

        private:
            void Publish(XJEditorUIState& state, bool canChangeSpace) const;
            XJEditorTransformSpace mSpace = XJEditorTransformSpace::Local;

            uint64_t mSettingsRevision = 0;
    };
}

#endif