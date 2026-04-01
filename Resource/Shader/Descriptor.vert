#version 450


layout(location  = 0) in vec3 _Pos;
layout(location  = 1) in vec2 _Texcoord;
layout(location  = 2) in vec3 _Normal;


out gl_PerVertex 
{
    vec4 gl_Position;
};

layout(set = 0, binding = 0, std140) uniform GlobalUbo 
{
    mat4 projMat;//投影矩阵
    mat4 viewMat;//视图矩阵
} globalUbo;

layout(set = 0, binding = 1, std140) uniform InstanceUbo
{
    mat4 modelMat;
} instanceUbo;

out layout(location = 1) vec2 v_Texcoord;

void main()
{
    //FIXME projMat[1][1] *= -1.0f;

    gl_Position = globalUbo.projMat * globalUbo.viewMat * instanceUbo.modelMat * vec4(_Pos.x, _Pos.y, _Pos.z, 1.0f);
    v_Texcoord = _Texcoord;
}