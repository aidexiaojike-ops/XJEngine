#ifndef XJ_FRAME_UBO_H
#define XJ_FRAME_UBO_H

#include "Edit/Mathinclude.h"

#include <cstddef>
#include <cstdint>

namespace XJ
{
    // 所有 Surface shader 共用的 set=0 FrameUbo std140 契约。
    struct XJFrameUbo
    {
        glm::mat4 projMat{1.0f};
        glm::mat4 viewMat{1.0f};
        alignas(8) glm::ivec2 resolution{0, 0};
        alignas(4) uint32_t frameId = 0;
        alignas(4) float time = 0.0f;
        alignas(16) glm::vec4 cameraPosition{0.0f}; // xyz=世界坐标
    };

    static_assert(sizeof(XJFrameUbo) == 160, "XJFrameUbo must match GLSL std140 size");
    static_assert(offsetof(XJFrameUbo, projMat) == 0);
    static_assert(offsetof(XJFrameUbo, viewMat) == 64);
    static_assert(offsetof(XJFrameUbo, resolution) == 128);
    static_assert(offsetof(XJFrameUbo, frameId) == 136);
    static_assert(offsetof(XJFrameUbo, time) == 140);
    static_assert(offsetof(XJFrameUbo, cameraPosition) == 144);
}

#endif
