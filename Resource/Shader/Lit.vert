#version 450

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 a_Texcoord;
layout(location = 2) in vec3 a_Normal;

layout(set = 0, binding = 0, std140) 
uniform FrameUbo
{
    mat4 projMat;
    mat4 viewMat;
    ivec2 resolution;
    uint frameId;
    float time;
    vec4 cameraPosition;
} frameUbo;

layout(push_constant)
uniform PushConstants
{
    mat4 modelMat;
    mat4 normalMat;
} PC;

layout(location = 1) out vec2 v_Texcoord;
layout(location = 2) out vec3 v_WorldNormal;
layout(location = 3) out vec3 v_WorldPosition;

void main()
{
    vec4 worldPosition = PC.modelMat * vec4(a_Pos, 1.0);

    v_Texcoord = a_Texcoord;
    v_WorldPosition = worldPosition.xyz;

    // normalMat 已由 CPU 写入 model matrix 的逆转置。
    v_WorldNormal = normalize(mat3(PC.normalMat) * a_Normal);

    gl_Position = frameUbo.projMat * frameUbo.viewMat * worldPosition;
}
