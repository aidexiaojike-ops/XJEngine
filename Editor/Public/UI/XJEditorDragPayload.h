#ifndef XJ_EDITOR_DRAG_PAYLOAD_H
#define XJ_EDITOR_DRAG_PAYLOAD_H


#include "Asset/XJAsset.h"
#include "Edit/Mathinclude.h"

namespace XJ
{
    constexpr const char* XJ_ASSET_PAYLOAD_NAME  = "XJ_ASSET_PAYLOAD";// 拖拽载荷类型常量

    struct XJAssetDragPayload// 拖拽载荷结构体
    {
        //结构
        XJAssetHandle Handle = 0;
        XJAssetType Type = XJAssetType::None;
        //射线   需要射线碰撞位置
        bool HasViewportRay = false;
        glm::vec3 RayOrigin{0.0f};
        glm::vec3 RayDirection{0.0f};
        //位置
        bool HasWorldPosition = false;
        glm::vec3 WorldPosition{0.0f};
        /* data */
    };
    
}


#endif