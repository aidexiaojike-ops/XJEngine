#ifndef XJ_EDITOR_ASSET_DRAG_PAYLOAD_H
#define XJ_EDITOR_ASSET_DRAG_PAYLOAD_H

#include "Asset/XJAsset.h"
//拖拽到UI

namespace XJ
{
    constexpr const char* XJ_ASSET_PAYLOAD_NAME = "XJ_ASSET_PAYLOAD";

    struct XJEditorAssetDragPayload
    {
        XJAssetHandle Handle = 0;
        XJAssetType Type = XJAssetType::None;
    };
}

#endif