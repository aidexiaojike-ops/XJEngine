#ifndef XJ_EDITOR_PLAY_MODE_H
#define XJ_EDITOR_PLAY_MODE_H

namespace XJ
{
    // ★★★ XJ_MARKER_PLAYMODE_STATE_ENUM_20260903 ★★★
    // 编辑器 Play Mode 状态机
    enum class XJEditorPlayState
    {
        Edit = 0,   // 编辑态
        Playing,    // 运行中
        Paused      // 暂停
    };
}

#endif