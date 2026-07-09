#ifndef XJ_SHADER_REFLECTION_UTILS_H
#define XJ_SHADER_REFLECTION_UTILS_H

#include "Render/Shader/XJShaderParameter.h"
#include "Render/Shader/XJShaderReflection.h"

#include <string>
//只负责 reflection 查找/参数类型规则
namespace XJ
{
    inline bool IsTextureParameter(XJShaderParameterType type)
    {
        return type == XJShaderParameterType::Texture2D || type == XJShaderParameterType::TextureCube;
    }

    inline uint32_t ExpectedMinimumParameterSize(XJShaderParameterType type)
    {
        switch (type)
        {
            case XJShaderParameterType::Float:
            case XJShaderParameterType::Int:
            case XJShaderParameterType::Bool:
                return 4;

            case XJShaderParameterType::Vec2:
                return 8;

            case XJShaderParameterType::Vec3:
            case XJShaderParameterType::Color3:
                return 12;

            case XJShaderParameterType::Vec4:
            case XJShaderParameterType::Color4:
                return 16;

            default:
                return 0;
        }
    }

    inline const XJShaderReflectedUbo* FindUbo(const XJShaderReflectionResult& reflection, const std::string& name)
    {
        for (const auto& ubo : reflection.Ubos)
        {
            if (ubo.Name == name)
                return &ubo;
        }
        return nullptr;
    }

    inline const XJShaderReflectedMember* FindMember(const XJShaderReflectedUbo& ubo, const std::string& name)
    {
        for (const auto& member : ubo.Members)
        {
            if (member.Name == name)
                return &member;
        }
        return nullptr;
    }

    inline const XJShaderReflectedSampler* FindSampler(const XJShaderReflectionResult& reflection, const std::string& name)
    {
        for (const auto& sampler : reflection.Samplers)
        {
            if (sampler.Name == name)
                return &sampler;
        }
        return nullptr;
    }
}

#endif