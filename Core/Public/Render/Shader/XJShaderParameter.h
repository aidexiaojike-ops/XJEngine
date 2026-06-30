#ifndef XJ_SHADER_PARAMETER_H//shader 用到的参数
#define XJ_SHADER_PARAMETER_H

#include "Asset/XJAsset.h"
#include "Edit/Mathinclude.h"

#include <string>
#include <variant>

namespace XJ//shader 用到的参数
{
    enum class XJShaderParameterType
    {
        None = 0,

        Float,
        Int,
        Bool,

        Vec2,
        Vec3,
        Vec4,

        Color3,
        Color4,

        Texture2D
    };

    using XJMaterialParameterValue = std::variant<std::monostate, float, int, bool, glm::vec2, glm::vec3, glm::vec4, XJAssetHandle>;

    struct XJParameterDef//数据默认
    {
        std::string Name;
        std::string DisplayName;
        XJShaderParameterType Type = XJShaderParameterType::None;

        XJMaterialParameterValue DefaultValue;

        bool HasRange = false;
        float Min = 0.0f;
        float Max = 1.0f;

        std::string Category;

        bool Editable = true;
        // UBO parameters use UboName + MemberName.
        // Texture parameters use SamplerName.
        std::string UboName;
        std::string MemberName;
        std::string SamplerName;
    };

    inline const char* XJShaderParameterTypeToString(XJShaderParameterType type)//转成字符串  UI用
    {
        switch (type)
        {
            case XJShaderParameterType::Float: return "float";
            case XJShaderParameterType::Int: return "int";
            case XJShaderParameterType::Bool: return "bool";
            case XJShaderParameterType::Vec2: return "vec2";
            case XJShaderParameterType::Vec3: return "vec3";
            case XJShaderParameterType::Vec4: return "vec4";
            case XJShaderParameterType::Color3: return "color3";
            case XJShaderParameterType::Color4: return "color4";
            case XJShaderParameterType::Texture2D: return "texture2D";
            default: return "none";
        }
    }

    inline XJShaderParameterType XJShaderParameterTypeFromString(const std::string& type)//字符串转成函数  数据用
    {
        if (type == "float") return XJShaderParameterType::Float;
        if (type == "int") return XJShaderParameterType::Int;
        if (type == "bool") return XJShaderParameterType::Bool;
        if (type == "vec2") return XJShaderParameterType::Vec2;
        if (type == "vec3") return XJShaderParameterType::Vec3;
        if (type == "vec4") return XJShaderParameterType::Vec4;
        if (type == "color3") return XJShaderParameterType::Color3;
        if (type == "color4") return XJShaderParameterType::Color4;
        if (type == "texture2D") return XJShaderParameterType::Texture2D;

        return XJShaderParameterType::None;
    }

}
#endif