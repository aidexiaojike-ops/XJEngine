#ifndef XJ_EDITOR_WORKSPACE_MODE_H
#define XJ_EDITOR_WORKSPACE_MODE_H

namespace XJ
{
    enum class XJEditorWorkspaceMode
    {
        Edit = 0,
        PlayReadOnly,
        PlayRuntime // 预留：未来让 Inspector 编辑 RuntimeScene。
    };
}

#endif
