#ifndef XJ_EDITOR_DRAG_PAYLOAD_H
#define XJ_EDITOR_DRAG_PAYLOAD_H

#include "Asset/XJAsset.h"
#include "Edit/Mathinclude.h"
#include "UI/XJEditorAssetDragPayload.h"

//拖拽到场景

namespace XJ
{
    struct XJAssetDragPayload : public XJEditorAssetDragPayload// 拖拽载荷结构体
    {
        //射线   需要射线碰撞位置
        bool HasViewportRay = false;
        glm::vec3 RayOrigin{0.0f};
        glm::vec3 RayDirection{0.0f};
        //位置
        bool HasWorldPosition = false;
        glm::vec3 WorldPosition{0.0f};

    };
    
}


#endif