//参数绑定解析结果
/*对单个 schema parameter 解析 reflection binding。
输出统一结构：
非纹理参数：UBO、member、set、binding、offset、size
纹理参数：sampler、set、binding
输出 errors/warnings。
Validator 用它生成 validation messages。
MaterialParameterLayout 用它生成 runtime layout。*/

#ifndef XJ_SHADER_SCHEMA_BINDING_RESOLVER_H
#define XJ_SHADER_SCHEMA_BINDING_RESOLVER_H

#include "Render/Shader/XJShaderParameter.h"
#include "Render/Shader/XJShaderReflection.h"
#include "Render/Shader/XJShaderReflectionUtils.h"
#include "Render/Shader/XJShaderSchema.h"

#include <string>
#include <vector>

namespace XJ
{
    enum class XJShaderResolvedBindingKind//
    {
        None = 0,
        UboMember,
        Texture
    };

    struct XJShaderResolvedBinding
    {
        XJShaderResolvedBindingKind Kind = XJShaderResolvedBindingKind::None;

        std::string ParameterName;
        XJShaderParameterType Type = XJShaderParameterType::None;

        std::string UboName;
        std::string MemberName;
        std::string SamplerName;

        uint32_t Set = 0;
        uint32_t Binding = 0;
        uint32_t Offset = 0;
        uint32_t Size = 0;
    };

    struct XJShaderSchemaBindingResolveResult
    {
        bool Valid = false;

        std::vector<XJShaderResolvedBinding> Bindings;
        std::vector<std::string> Errors;
        std::vector<std::string> Warnings;
    };

    inline void AddSchemaBindingError(XJShaderSchemaBindingResolveResult& result, const std::string& message)
    {
        result.Errors.push_back(message);
    }

    inline void AddSchemaBindingWarning(XJShaderSchemaBindingResolveResult& result, const std::string& message)
    {
        result.Warnings.push_back(message);
    }

    inline XJShaderSchemaBindingResolveResult ResolveShaderSchemaBindings(const XJShaderSchema& schema, const XJShaderReflectionResult& reflection)
    {
        XJShaderSchemaBindingResolveResult result;

        if(!reflection.Valid)
        {
            //如果反射结果无效，则添加错误消息并返回结果
            AddSchemaBindingWarning(result, "Shader reflection is invalid.");
            for(const auto& error : reflection.Errors)
                AddSchemaBindingError(result, error);

            result.Valid = false;
            return result;
        }

        for(const auto& parameter : schema.Parameters)
        {
            //纹理参数
            if(IsTextureParameter(parameter.Type))
            {
                if(parameter.SamplerName.empty())
                {
                    AddSchemaBindingWarning(result, "Texture parameter has no sampler binding: " + parameter.Name);
                    continue;
                }

                const XJShaderReflectedSampler* sampler = FindSampler(reflection, parameter.SamplerName);
                if (!sampler)
                {
                    AddSchemaBindingError(result, "Sampler not found in SPIR-V reflection: " + parameter.Name + " -> " + parameter.SamplerName);
                    continue;
                }

                XJShaderResolvedBinding binding;
                binding.Kind = XJShaderResolvedBindingKind::Texture;
                binding.ParameterName = parameter.Name;
                binding.Type = parameter.Type;
                binding.SamplerName = parameter.SamplerName;
                binding.Set = sampler->Set;
                binding.Binding = sampler->Binding;

                result.Bindings.push_back(binding);
                continue;
            }
            // 非纹理参数
            if (parameter.UboName.empty() || parameter.MemberName.empty())
            {
                AddSchemaBindingWarning(result, "Non-texture parameter has no ubo/member binding: " + parameter.Name);
                continue;
            }
            // 查找 UBO
            const XJShaderReflectedUbo* ubo = FindUbo(reflection, parameter.UboName);
            if (!ubo)
            {
                AddSchemaBindingError(result, "UBO not found in SPIR-V reflection: " + parameter.Name + " -> " + parameter.UboName);
                continue;
            }
             // 查找 UBO 中的成员
            const XJShaderReflectedMember* member = FindMember(*ubo, parameter.MemberName);
            if (!member)
            {
                AddSchemaBindingError(result, "UBO member not found in SPIR-V reflection: " + parameter.Name + " -> " + parameter.UboName + "." + parameter.MemberName);
                continue;
            }
            // 检查成员的大小是否符合预期的参数类型
            const uint32_t expectedSize = ExpectedMinimumParameterSize(parameter.Type);
            if (expectedSize != 0 && member->Size != 0 && member->Size < expectedSize)
            {
                AddSchemaBindingError(result, "UBO member size is smaller than expected: " + parameter.Name + " -> " + parameter.UboName + "." + parameter.MemberName);
                continue;
            }

            XJShaderResolvedBinding binding;
            binding.Kind = XJShaderResolvedBindingKind::UboMember;
            binding.ParameterName = parameter.Name;
            binding.Type = parameter.Type;
            binding.UboName = ubo->Name;
            binding.MemberName = member->Name;
            binding.Set = ubo->Set;
            binding.Binding = ubo->Binding;
            binding.Offset = member->Offset;
            binding.Size = member->Size;

            result.Bindings.push_back(binding);
        }

        result.Valid = result.Errors.empty();
        return result;
    }
}

#endif