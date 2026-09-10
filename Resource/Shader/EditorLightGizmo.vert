#version 450

layout(location = 0) in vec3 a_Position;

layout(push_constant) uniform LightGizmoPushConstants
{
    mat4 mvp;
    vec4 color;
} pc;

layout(location = 0) out vec4 v_Color;

void main()
{
    v_Color = pc.color;
    gl_Position = pc.mvp * vec4(a_Position, 1.0);
}
