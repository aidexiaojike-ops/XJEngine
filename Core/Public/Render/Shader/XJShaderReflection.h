#ifndef XJ_SHADER_REFLECTION_H
#define XJ_SHADER_REFLECTION_H

#include <cstdint>
#include <string>
#include <vector>

namespace XJ
{
    enum class XJShaderStage//SHADER阶段？
    {
        Unknown = 0,
        Vertex,
        Fragment,
        Compute
    };

    struct XJShaderReflectedMember//反射成员
    {
        std::string Name;
        uint32_t Offset = 0;
        uint32_t Size = 0;
    };

    struct XJShaderReflectedUbo//反射 UBO
    {
        std::string Name;
        uint32_t Set = 0;
        uint32_t Binding = 0;
        uint32_t Size = 0;
        XJShaderStage Stage = XJShaderStage::Unknown;
        std::vector<XJShaderReflectedMember> Members;
    };

    struct XJShaderReflectedSampler//反射采样器
    {
        std::string Name;
        uint32_t Set = 0;
        uint32_t Binding = 0;
        XJShaderStage Stage = XJShaderStage::Unknown;
    };

    struct XJShaderReflectionResult//反射结果
    {
        bool Valid = false;
        std::vector<XJShaderReflectedUbo> Ubos;
        std::vector<XJShaderReflectedSampler> Samplers;
        std::vector<std::string> Errors;
    };
}


#endif