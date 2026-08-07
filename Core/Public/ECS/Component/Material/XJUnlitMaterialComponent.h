#ifndef XJ_UNLIT_MATERIAL_COMPONENT_H
#define XJ_UNLIT_MATERIAL_COMPONENT_H

#include "ECS/Component/Material/XJMaterialComponent.h"

#include <cstddef>


namespace XJ
{

    enum UnlitMaterialTexture
    {
        UNLIT_MAT_BASE_COLOR   
    };

    struct FrameUbo
    {
        glm::mat4  projMat{ 1.f };            // offset   0  size 64
        glm::mat4  viewMat{ 1.f };            // offset  64  size 64
        alignas(8) glm::ivec2 resolution;     // offset 128  size 8   （std140: ivec2 对齐 8）
        alignas(4) uint32_t frameId;          // offset 136  size 4
        alignas(4) float time;                // offset 140  size 4
    };

    // 与 Shader/Unlit.vert、Unlit.frag 的 `layout(set=0, binding=0, std140) uniform FrameUbo` 逐字节一致。
    // 若改动 shader 中该块的成员顺序/类型，必须同步这里的对齐与 static_assert，否则 GPU 侧会静默错位。
    static_assert(sizeof(FrameUbo) == 144,               "FrameUbo: sizeof 必须为 144（std140，按 16 字节对齐取整）");
    static_assert(offsetof(FrameUbo, projMat) == 0,      "FrameUbo.projMat 偏移必须为 0");
    static_assert(offsetof(FrameUbo, viewMat) == 64,     "FrameUbo.viewMat 偏移必须为 64");
    static_assert(offsetof(FrameUbo, resolution) == 128, "FrameUbo.resolution 偏移必须为 128");
    static_assert(offsetof(FrameUbo, frameId) == 136,    "FrameUbo.frameId 偏移必须为 136");
    static_assert(offsetof(FrameUbo, time) == 140,       "FrameUbo.time 偏移必须为 140");


    
    class XJUnlitMaterial : public XJMaterial
    {
        public:
            //设置材质参数值
            void SetBaseColor(const glm::vec4& color)
            {
                SetPrimaryUboMemberValue("baseColor", XJShaderParameterType::Color4, color);
            }

          
    };

    class XJUnlitMaterialComponent : public XJMaterialComponent<XJUnlitMaterial>//runtime render data
    {
        private:
            /* data */
        public:
       
    };
    

    
}

#endif
