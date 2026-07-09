#include "Render/Material/XJMaterialParameterBlockWriter.h"

namespace XJ
{
    bool WriteMaterialParameterValueToBlock(XJMaterialParameterBlock& block, const XJMaterialParameterBinding& binding, const XJMaterialParameterValue& value)
    {
        switch (binding.Type)
        {
            case XJShaderParameterType::Float:
            {
                if (std::holds_alternative<float>(value))
                    return block.SetFloat(binding.Offset, std::get<float>(value));

                return false;
            }

            case XJShaderParameterType::Int:
            {
                if (std::holds_alternative<int>(value))
                    return block.SetInt(binding.Offset, std::get<int>(value));

                return false;
            }

            case XJShaderParameterType::Bool:
            {
                if (std::holds_alternative<bool>(value))
                    return block.SetBool(binding.Offset, std::get<bool>(value));

                return false;
            }

            case XJShaderParameterType::Vec2:
            {
                if (std::holds_alternative<glm::vec2>(value))
                    return block.SetVec2(binding.Offset, std::get<glm::vec2>(value));

                return false;
            }

            case XJShaderParameterType::Vec3:
            case XJShaderParameterType::Color3:
            {
                if (std::holds_alternative<glm::vec3>(value))
                    return block.SetVec3(binding.Offset, std::get<glm::vec3>(value));

                return false;
            }

            case XJShaderParameterType::Vec4:
            case XJShaderParameterType::Color4:
            {
                if (std::holds_alternative<glm::vec4>(value))
                    return block.SetVec4(binding.Offset, std::get<glm::vec4>(value));

                return false;
            }

            case XJShaderParameterType::Texture2D:
            case XJShaderParameterType::TextureCube:
            default:
                return false;
        }
    }
}