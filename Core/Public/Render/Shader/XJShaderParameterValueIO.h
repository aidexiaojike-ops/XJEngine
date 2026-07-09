#ifndef XJ_SHADER_PARAMETER_VALUE_IO_H
#define XJ_SHADER_PARAMETER_VALUE_IO_H

#include "Render/Shader/XJShaderParameter.h"

#include <cstdint>
#include <nlohmann/json.hpp>
//序列化/反序列化
//只负责 JSON <-> XJMaterialParameterValue
namespace XJ
{
    inline XJMaterialParameterValue ReadShaderParameterValue(//读取材质参数值
        const nlohmann::json& value,
        XJShaderParameterType type,
        const glm::vec4& invalidVec4Fallback = glm::vec4(0.0f))
    {
        switch (type)
        {
            case XJShaderParameterType::Float:
                return value.is_number() ? value.get<float>() : 0.0f;

            case XJShaderParameterType::Int:
                return value.is_number_integer() ? value.get<int>() : 0;

            case XJShaderParameterType::Bool:
                return value.is_boolean() ? value.get<bool>() : false;

            case XJShaderParameterType::Vec2:
            {
                if (!value.is_array() || value.size() < 2)
                    return glm::vec2(0.0f);

                return glm::vec2(value[0].get<float>(), value[1].get<float>());
            }

            case XJShaderParameterType::Vec3:
            case XJShaderParameterType::Color3:
            {
                if (!value.is_array() || value.size() < 3)
                    return glm::vec3(0.0f);

                return glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
            }

            case XJShaderParameterType::Vec4:
            case XJShaderParameterType::Color4:
            {
                if (!value.is_array() || value.size() < 4)
                    return invalidVec4Fallback;

                return glm::vec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
            }

            case XJShaderParameterType::Texture2D:
            case XJShaderParameterType::TextureCube:
            {
                if (value.is_number_unsigned())
                    return static_cast<XJAssetHandle>(value.get<uint64_t>());

                if (value.is_number_integer())
                    return static_cast<XJAssetHandle>(value.get<int64_t>());

                return static_cast<XJAssetHandle>(0);
            }

            default:
                return std::monostate{};
        }
    }

    inline nlohmann::json WriteShaderParameterValue(const XJMaterialParameterValue& value)//写材质参数值
    {
        if (std::holds_alternative<float>(value))
            return std::get<float>(value);

        if (std::holds_alternative<int>(value))
            return std::get<int>(value);

        if (std::holds_alternative<bool>(value))
            return std::get<bool>(value);

        if (std::holds_alternative<glm::vec2>(value))
        {
            const auto& v = std::get<glm::vec2>(value);
            return nlohmann::json::array({ v.x, v.y });
        }

        if (std::holds_alternative<glm::vec3>(value))
        {
            const auto& v = std::get<glm::vec3>(value);
            return nlohmann::json::array({ v.x, v.y, v.z });
        }

        if (std::holds_alternative<glm::vec4>(value))
        {
            const auto& v = std::get<glm::vec4>(value);
            return nlohmann::json::array({ v.x, v.y, v.z, v.w });
        }

        if (std::holds_alternative<XJAssetHandle>(value))
            return static_cast<uint64_t>(std::get<XJAssetHandle>(value));

        return nullptr;
    }
}

#endif