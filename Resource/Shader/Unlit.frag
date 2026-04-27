#version 450 

layout(location = 1) in vec2 v_Texcoord;

struct TextureParam
{
    bool  enable;
    float uvRotation;//
    vec4  uvTransform;//x,y  ->scale, z,w->translation
};

vec2 getTextureUV(TextureParam param, vec2 inUV)
{
    vec2 retUV = inUV * param.uvTransform.xy;//uv缩放
    retUV = vec2
    (
        retUV.x * sin(param.uvRotation) + retUV.y * cos(param.uvRotation),
        retUV.y * sin(param.uvRotation) + retUV.x * cos(param.uvRotation)
    );//uv旋转
    inUV = retUV + param.uvTransform.zw;//uv 位移
    return inUV;
}

layout(set = 0, binding = 0, std140) uniform FrameUbo
{
    mat4 projMat;
    mat4 viewMat;
    ivec2 resolution;
    uint frameId;
    float time;
} frameUbo;

layout(set = 1, binding = 0, std140) uniform MaterialUbo
{
    vec3 baseColorA;
    vec3 baseColorB;
    float mixValue;
    TextureParam textureParamA;
    TextureParam textureParamB;
}materialUbo;

layout(set = 2, binding = 0) uniform sampler2D textureA;
layout(set = 2, binding = 1) uniform sampler2D textureB;


layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 colorA = materialUbo.baseColorA;
    vec3 colorB = materialUbo.baseColorB;

    if(materialUbo.textureParamA.enable)
    {
        TextureParam param = materialUbo.textureParamA;
        param.uvTransform.w = -frameUbo.time;
        colorA = texture(textureA, getTextureUV(param, v_Texcoord)).rgb;   
    }

    if(materialUbo.textureParamB.enable)
    {
        colorB = texture(textureB, getTextureUV(materialUbo.textureParamB, v_Texcoord)).rgb;  
    }

    vec3 finalColor = mix(colorA, colorB, materialUbo.mixValue);
    fragColor = vec4(finalColor, 1.0);

}