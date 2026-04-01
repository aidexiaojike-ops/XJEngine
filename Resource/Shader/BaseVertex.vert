#version 450


layout(location  = 0) in vec3 _Pos;
layout(location  = 1) in vec2 _Texcoord;
layout(location  = 2) in vec3 _Normal;


out gl_PerVertex 
{
    vec4 gl_Position;
};

layout(push_constant) uniform PushConstants
{
    mat4 matrix;
    uint colorType;
} PC;

out layout(location = 0) vec4 vectexColor;

void main()
{
    gl_Position = PC.matrix * vec4(_Pos, 1.0f);
    vectexColor = PC.colorType == 0 ? vec4(_Normal.x *0.5 + 0.5, _Normal.y *0.5 + 0.5, _Normal.z *0.5 + 0.5,1.0f) : vec4(_Texcoord, 1.0f, 1.0f);
}