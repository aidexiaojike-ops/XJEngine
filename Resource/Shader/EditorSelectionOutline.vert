#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec3 a_Normal;

layout(push_constant) uniform OutlinePushConstants
{
    mat4 mvp;

    vec4 normalColumn0;
    vec4 normalColumn1;
    vec4 normalColumn2;

    // rgb = 轮廓颜色，w = NDC 厚度。
    vec4 color;
} pc;

layout(location = 0) out vec4 v_Color;

void main()
{
    vec4 clip =
        pc.mvp * vec4(a_Position, 1.0);

    mat3 normalToClip = mat3(
        pc.normalColumn0.xyz,
        pc.normalColumn1.xyz,
        pc.normalColumn2.xyz);

    vec2 direction =
        (normalToClip * a_Normal).xy;

    float lengthSquared =
        dot(direction, direction);

    // 朝向相机的法线在屏幕上没有扩张方向。
    // 轮廓主要来自侧向法线。
    if (lengthSquared > 1.0e-10)
    {
        direction *= inversesqrt(lengthSquared);

        clip.xy +=
            direction *
            clip.w *
            pc.color.w;
    }

    v_Color = vec4(pc.color.rgb, 1.0);

    gl_Position = clip;
}