#ifndef XJ_LIGHT_UBO_H
#define XJ_LIGHT_UBO_H

#include "Edit/Mathinclude.h"
#include <cstddef>
#include <cstdint>

namespace XJ
{
    constexpr uint32_t XJ_MAX_POINT_LIGHTS = 8;
    constexpr uint32_t XJ_MAX_SPOT_LIGHTS = 8;
    // std140 布局，与 Shader 中 LightUbo 逐字节一致。
    struct XJDirectionalLightData
    {
        alignas(16) glm::vec4 DirectionIntensity; /// xyz=方向(normalized), w=强度
        alignas(16) glm::vec4 ColorEnabled; /// rgb=颜色,  a=启用(0/1)
    };
    
    static_assert(sizeof(XJDirectionalLightData) == 32, "XJDirectionalLightData: size must be 32 bytes");
    static_assert(alignof(XJDirectionalLightData) == 16, "XJDirectionalLightData: alignment must be 16 bytes");

    struct XJPointLightData
    {
        alignas(16) glm::vec4 PositionIntensity; // xyz=位置, w=强度
        alignas(16) glm::vec4 ColorEnabled;      // rgb=颜色, a=启用
        alignas(16) glm::vec4 RangeData;         // x=影响半径
        alignas(16) glm::vec4 Reserved;
    };
    static_assert(sizeof(XJPointLightData) == 64, "PointLight: 64 字节(std140)");
    static_assert(alignof(XJPointLightData) == 16, "PointLight: alignment must be 16 bytes");
    static_assert(offsetof(XJPointLightData, PositionIntensity) == 0);
    static_assert(offsetof(XJPointLightData, ColorEnabled) == 16);
    static_assert(offsetof(XJPointLightData, RangeData) == 32);
    static_assert(offsetof(XJPointLightData, Reserved) == 48);

    struct XJSpotLightData
    {
        alignas(16) glm::vec4 PositionIntensity; // xyz=位置, w=强度
        alignas(16) glm::vec4 ColorEnabled;      // rgb=颜色, a=启用
        // 聚光角存半角余弦；std140 每项按 vec4 槽位上传。
        alignas(16) glm::vec4 DirectionAngle;    // xyz=方向, w=cos(内角)
        alignas(16) glm::vec4 RangeOuter;        // x=范围, y=cos(外角)
    };
    static_assert(sizeof(XJSpotLightData) == 64, "SpotLight: 64 字节(std140)");
    static_assert(alignof(XJSpotLightData) == 16, "SpotLight: alignment must be 16 bytes");

    struct XJLightUbo
    {
        XJDirectionalLightData Directional;
        XJPointLightData Points[XJ_MAX_POINT_LIGHTS];
        XJSpotLightData Spots[XJ_MAX_SPOT_LIGHTS];
        alignas(4) uint32_t DirectionalCount; // 0/1
        alignas(4) uint32_t PointCount;
        alignas(4) uint32_t SpotCount;
        alignas(4) uint32_t Padding;
    };

    static_assert(offsetof(XJLightUbo, DirectionalCount) == 32 + 64 * XJ_MAX_POINT_LIGHTS + 64 * XJ_MAX_SPOT_LIGHTS);
    static_assert(offsetof(XJLightUbo, PointCount) == offsetof(XJLightUbo, DirectionalCount) + 4);
    static_assert(offsetof(XJLightUbo, SpotCount) == offsetof(XJLightUbo, PointCount) + 4);
    static_assert(offsetof(XJLightUbo, Padding) == offsetof(XJLightUbo, SpotCount) + 4);
    static_assert(sizeof(XJLightUbo) == 32 + 64 * XJ_MAX_POINT_LIGHTS + 64 * XJ_MAX_SPOT_LIGHTS + 16, "XJLightUbo: size must be 32 + 64 * XJ_MAX_POINT_LIGHTS + 64 * XJ_MAX_SPOT_LIGHTS + 16 bytes");

}


#endif // XJ_LIGHT_UBO_H
