#version 450

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_GridCoordinate;

layout(location = 0) out vec4 o_Color;

void main()
{
    // 圆形边缘淡出，隐藏有限网格的硬边。
    float distanceToCenter = length(v_GridCoordinate);

    float fade =
        1.0 - smoothstep(
            0.70,
            1.00,
            distanceToCenter);

    float alpha = v_Color.a * fade;

    if (alpha <= 0.005)
        discard;

    o_Color = vec4(v_Color.rgb, alpha);
}