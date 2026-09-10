#version 450
//最多8个灯光
#define XJ_MAX_POINT_LIGHTS 8
#define XJ_MAX_SPOT_LIGHTS 8
//参数
layout(location = 1) in vec2 v_Texcoord;
layout(location = 2) in vec3 v_WorldNormal;
layout(location = 3) in vec3 v_WorldPosition;

layout(location = 0) out vec4 o_FragColor;

struct TextureParam
{
    uint enable;
    float uvRotation;
    vec4 uvTransform;
};

struct DirectionalLightData
{
    vec4 directionIntensity;
    vec4 colorEnabled;
};

struct PointLightData
{
    vec4 positionIntensity;
    vec4 colorEnabled;
    vec4 rangeData;
    vec4 reserved;
};

struct SpotLightData
{
    vec4 positionIntensity;
    vec4 colorEnabled;
    vec4 directionAngle;
    vec4 rangeOuter;
};

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

layout(set = 1, binding = 0, std140)
uniform MaterialUbo
{
    vec4 baseColor;
    TextureParam textureParam;
    float specularStrength;
    float shininess;
} materialUbo;

layout(set = 2, binding = 0)
uniform sampler2D albedoTexture;

layout(set = 3, binding = 0, std140)
uniform LightUbo
{
    DirectionalLightData directionalLight;
    PointLightData pointLights[XJ_MAX_POINT_LIGHTS];
    SpotLightData spotLights[XJ_MAX_SPOT_LIGHTS];

    uint directionalLightCount;
    uint pointLightCount;
    uint spotLightCount;
    uint padding;
} lightUbo;
//算法
vec2 GetTextureUV(TextureParam parameter, vec2 uv)
{
    vec2 transformed = uv * parameter.uvTransform.xy;
    float sineValue = sin(parameter.uvRotation);
    float cosineValue = cos(parameter.uvRotation);

    transformed = mat2(cosineValue, -sineValue, sineValue, cosineValue)*transformed;

    return transformed + parameter.uvTransform.zw;
}

vec3 EvaluateLight( vec3 lightDirection, vec3 radiance, vec3 normal, vec3 viewDirection, vec3 albedo)
{
    float diffuseFactor = max(dot(normal, lightDirection),0.0);
    vec3 diffuse = albedo * diffuseFactor;
    if (diffuseFactor <= 0.0)
        return vec3(0.0);

    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float specularFactor = pow(max(dot(normal, halfDirection),0.0),max(materialUbo.shininess,1.0));
    vec3 specular = vec3(materialUbo.specularStrength)* specularFactor;

    return (diffuse + specular)*radiance;
}

float RangeAttenuation(float distanceToLight, float range)
{
    if(range <= 0.0 || distanceToLight >= range)
    {
        return 0.0;
    }

    float normalizedDistance = distanceToLight / range;
    float attenuation = 1.0 - normalizedDistance;

    return attenuation * attenuation;
}

void main()
{
    vec4 surfaceColor = materialUbo.baseColor;

    if(materialUbo.textureParam.enable != 0)
    {
        surfaceColor *= texture(albedoTexture, GetTextureUV(materialUbo.textureParam, v_Texcoord));
    }

    vec3 normal = normalize(v_WorldNormal);
    vec3 viewDirection = normalize(frameUbo.cameraPosition.xyz - v_WorldPosition);
    // 少量环境光，避免无灯光区域完全黑色。
    vec3 result = surfaceColor.rgb * 0.03;

    if(lightUbo.directionalLightCount > 0 && lightUbo.directionalLight.colorEnabled.a > 0.5)
    {
        // UBO 保存光线传播方向，指向光源的 L 需要取反。
        vec3 lightDirection = normalize(-lightUbo.directionalLight.directionIntensity.xyz);
        vec3 radiance = lightUbo.directionalLight.colorEnabled.rgb * lightUbo.directionalLight.directionIntensity.w;

        result += EvaluateLight(lightDirection, radiance, normal, viewDirection, surfaceColor.rgb);
    }
    //计算所有点光源
    uint pointCount = min(lightUbo.pointLightCount, uint(XJ_MAX_POINT_LIGHTS));
    for(uint index = 0; index < pointCount; ++index)
    {
        PointLightData light = lightUbo.pointLights[index];

        vec3 toLight = light.positionIntensity.xyz - v_WorldPosition;

        float distanceToLight = length(toLight);
        float attenuation = RangeAttenuation(distanceToLight, light.rangeData.x);

        if(attenuation <= 0.0)
            continue;

        vec3 lightDirection = toLight/max(distanceToLight,0.0001);
        vec3 radiance = light.colorEnabled.rgb * light.positionIntensity.w * attenuation;

        result += EvaluateLight(lightDirection,radiance,normal,viewDirection,surfaceColor.rgb);

    }
    //计算所有spot 光
    uint spotCount = min(lightUbo.spotLightCount, uint(XJ_MAX_SPOT_LIGHTS));
    for(uint index = 0; index < spotCount; ++index)
    {
        SpotLightData light = lightUbo.spotLights[index];

        vec3 fromLight = v_WorldPosition - light.positionIntensity.xyz;
        float distanceToLight = length(fromLight);
        float rangeAttenuation = RangeAttenuation(distanceToLight, light.rangeOuter.x);

        if (rangeAttenuation <= 0.0)
            continue;

        vec3 fromLightDirection = fromLight / max(distanceToLight, 0.0001);
        float coneCosine = dot(fromLightDirection, normalize(light.directionAngle.xyz));
        float angleAttenuation = smoothstep(light.rangeOuter.y, light.directionAngle.w, coneCosine);

        if (angleAttenuation <= 0.0)
            continue;

        vec3 lightDirection = -fromLightDirection;
        vec3 radiance = light.colorEnabled.rgb * light.positionIntensity.w * rangeAttenuation* angleAttenuation;

        result += EvaluateLight(lightDirection,radiance, normal,viewDirection, surfaceColor.rgb);
    }
    o_FragColor = vec4(result, surfaceColor.a);
}
