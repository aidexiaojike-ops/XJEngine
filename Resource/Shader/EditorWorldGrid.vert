#version 450

layout(push_constant) uniform WorldGridPushConstants
{
    mat4 viewProjection;

    vec4 anchor;
    vec4 right;
    vec4 up;

    uvec4 data;
} pc;

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_GridCoordinate;

// 一个字符最多使用 16 条线段。
// vec4 = 起点.xy、终点.zw。
const vec4 SEGMENTS[16] = vec4[16](
    vec4(0.0, 1.0, 0.6, 1.0),  // 0: 上
    vec4(0.6, 1.0, 0.6, 0.5),  // 1: 右上
    vec4(0.6, 0.5, 0.6, 0.0),  // 2: 右下
    vec4(0.0, 0.0, 0.6, 0.0),  // 3: 下
    vec4(0.0, 0.0, 0.0, 0.5),  // 4: 左下
    vec4(0.0, 0.5, 0.0, 1.0),  // 5: 左上
    vec4(0.0, 0.5, 0.6, 0.5),  // 6: 中
    vec4(0.24, 0.0, 0.30, 0.0),// 7: 小数点
    vec4(0.0, 0.0, 0.0, 0.65), // 8: m 左
    vec4(0.6, 0.0, 0.6, 0.65), // 9: m 右
    vec4(0.3, 0.0, 0.3, 0.65), // 10: m 中
    vec4(0.0, 0.65, 0.6, 0.65),// 11: m 顶
    vec4(0.0, 0.65, 0.6, 0.65),// 12: = 上
    vec4(0.0, 0.35, 0.6, 0.35),// 13: = 下
    vec4(0.0),
    vec4(0.0)
);

// 0~9、-、.、m、=、G、空白。
const uint GLYPH_MASKS[16] = uint[16](
    63u,     // 0
    6u,      // 1
    91u,     // 2
    79u,     // 3
    102u,    // 4
    109u,    // 5
    125u,    // 6
    7u,      // 7
    127u,    // 8
    111u,    // 9
    64u,     // -
    128u,    // .
    3840u,   // m
    12288u,  // =
    61u,     // G：开放式线字
    0u       // 空白
);

void BuildGridVertex()
{
    int halfCount = int(pc.data.y);
    int linesPerDirection = halfCount * 2 + 1;

    int lineIndex = gl_VertexIndex / 2;
    int endpoint = gl_VertexIndex % 2;

    bool parallelToX =
        lineIndex < linesPerDirection;

    int localIndex =
        parallelToX
            ? lineIndex
            : lineIndex - linesPerDirection;

    float step = pc.anchor.w;
    float radius = float(halfCount) * step;

    float offset =
        float(localIndex - halfCount) * step;

    float along =
        endpoint == 0 ? -radius : radius;

    vec3 position = pc.anchor.xyz;

    if (parallelToX)
    {
        position.x += along;
        position.z += offset;
    }
    else
    {
        position.x += offset;
        position.z += along;
    }

    float fixedCoordinate =
        parallelToX ? position.z : position.x;

    bool isAxis =
        abs(fixedCoordinate) < step * 0.01;

    // 网格坐标范围受 CPU 限制，转 int 不会溢出。
    int gridIndex =
        int(round(fixedCoordinate / step));

    bool isMajor =
        (gridIndex % 5) == 0;

    if (isAxis)
    {
        v_Color =
            parallelToX
                ? vec4(0.95, 0.25, 0.22, 0.95)
                : vec4(0.25, 0.48, 1.00, 0.95);
    }
    else if (isMajor)
    {
        v_Color = vec4(0.55, 0.58, 0.63, 0.65);
    }
    else
    {
        v_Color = vec4(0.43, 0.46, 0.51, 0.30);
    }

    v_GridCoordinate =
        (position.xz - pc.anchor.xz) / radius;

    gl_Position =
        pc.viewProjection * vec4(position, 1.0);
}

void BuildLabelVertex()
{
    uint mode = pc.data.x;

    v_Color =
        mode == 1u
            ? vec4(1.00, 0.42, 0.36, 1.0)
            : mode == 2u
                ? vec4(0.42, 0.66, 1.00, 1.0)
                : vec4(0.90, 0.90, 0.82, 1.0);

    // 标签不使用网格边缘淡出。
    v_GridCoordinate = vec2(0.0);

    // 前两个顶点：主轴短刻线。
    if (gl_VertexIndex < 2)
    {
        float signValue =
            gl_VertexIndex == 0 ? -1.0 : 1.0;

        vec3 direction =
            mode == 1u
                ? vec3(0.0, 0.0, 1.0)
                : vec3(1.0, 0.0, 0.0);

        // 格距说明不画刻线。
        float lengthValue =
            mode == 3u ? 0.0 : pc.anchor.w;

        vec3 position =
            pc.anchor.xyz +
            direction * lengthValue * signValue;

        gl_Position =
            pc.viewProjection * vec4(position, 1.0);

        return;
    }

    int vertexIndex = gl_VertexIndex - 2;

    uint characterIndex =
        uint(vertexIndex / 32);

    uint segmentIndex =
        uint((vertexIndex % 32) / 2);

    bool secondEndpoint =
        (vertexIndex % 2) != 0;

    uint packed =
        characterIndex < 8u
            ? pc.data.z
            : pc.data.w;

    uint shift =
        (characterIndex % 8u) * 4u;

    uint glyph =
        (packed >> shift) & 15u;

    uint mask =
        GLYPH_MASKS[glyph];

    vec4 segment =
        SEGMENTS[segmentIndex];

    bool enabled =
        (mask & (1u << segmentIndex)) != 0u;

    // 未使用的线段两端重合，形成退化线段。
    vec2 point =
        enabled
            ? (secondEndpoint ? segment.zw : segment.xy)
            : vec2(0.0);

    float characterAdvance = 0.8;

    float totalWidth =
        pc.data.y == 0u
            ? 0.0
            : float(pc.data.y - 1u) *
              characterAdvance + 0.6;

    point.x +=
        float(characterIndex) * characterAdvance -
        totalWidth * 0.5;

    // 文字位于锚点的屏幕上方。
    point.y += 0.35;

    vec3 position =
        pc.anchor.xyz +
        pc.right.xyz * point.x +
        pc.up.xyz * point.y;

    gl_Position =
        pc.viewProjection * vec4(position, 1.0);
}

void main()
{
    if (pc.data.x == 0u)
        BuildGridVertex();
    else
        BuildLabelVertex();
}