#include "Render/Material/XJMaterialParameterLayout.h"
#include "Render/Shader/XJShaderReflectionUtils.h"
#include "Render/Material/XJMaterialBuildResultUtils.h"

namespace XJ
{

    XJMaterialParameterLayoutBuildResult XJMaterialParameterLayout::Build(const XJShaderSchema& schema, const XJShaderReflectionResult& reflection)
    {
        Clear();

        XJMaterialParameterLayoutBuildResult buildResult;

        if(!reflection.Valid)
        {
            AddMaterialBuildError(buildResult, "Cannot build material parameter layout: shader reflection is invalid.");

            for (const auto& error : reflection.Errors)
                AddMaterialBuildError(buildResult, error);

            return buildResult;
        }

        for(const auto& parameter : schema.Parameters)
        {
            if(IsTextureParameter(parameter.Type))
            {
                if(parameter.SamplerName.empty())
                {
                    AddMaterialBuildError(buildResult, "Texture parameter '" + parameter.Name + "' is missing a sampler name.");
                    continue;
                }

                const XJShaderReflectedSampler* sampler = FindSampler(reflection, parameter.SamplerName);
                if (!sampler)
                {
                    AddMaterialBuildError(buildResult, "Sampler not found in reflection for parameter: " + parameter.Name + " -> " + parameter.SamplerName);
                    continue;
                }

                XJMaterialTextureBinding textureBinding;
                textureBinding.Type = parameter.Type;
                textureBinding.ParameterName = parameter.Name;
                textureBinding.SamplerName = parameter.SamplerName;
                textureBinding.Set = sampler->Set;
                textureBinding.Binding = sampler->Binding;

                mTextureBindings.push_back(textureBinding);
                continue;
            }

            if (parameter.UboName.empty() || parameter.MemberName.empty())
            {
                AddMaterialBuildError(buildResult, "Non-texture parameter has no ubo/member binding: " + parameter.Name);
                continue;
            }
            const XJShaderReflectedUbo* ubo = FindUbo(reflection, parameter.UboName);
            if (!ubo)
            {
                AddMaterialBuildError(buildResult, "UBO not found in reflection for parameter: " + parameter.Name + " -> " + parameter.UboName);
                continue;
            }

            const XJShaderReflectedMember* member = FindMember(*ubo, parameter.MemberName);
            if (!member)
            {
                AddMaterialBuildError(buildResult, "UBO member not found in reflection for parameter: " + parameter.Name + " -> " + parameter.UboName + "." + parameter.MemberName);
                continue;
            }

            if (mUboName.empty())
            {
                mUboName = ubo->Name;
                mUboSize = ubo->Size;
            }
            else if (mUboName != ubo->Name)
            {
                AddMaterialBuildError(buildResult, "Multiple material UBOs are not fully supported yet. Parameter uses UBO: " + parameter.UboName);
            }

            const uint32_t expectedSize = ExpectedMinimumParameterSize(parameter.Type);
            if (expectedSize != 0 && member->Size != 0 && member->Size < expectedSize)
            {
                AddMaterialBuildError(buildResult, "Reflected member size is smaller than expected for parameter: " + parameter.Name);
            }

            XJMaterialParameterBinding parameterBinding;
            parameterBinding.ParameterName = parameter.Name;
            parameterBinding.Type = parameter.Type;
            parameterBinding.UboName = parameter.UboName;
            parameterBinding.MemberName = parameter.MemberName;
            parameterBinding.Set = ubo->Set;
            parameterBinding.Binding = ubo->Binding;
            parameterBinding.Offset = member->Offset;
            parameterBinding.Size = member->Size;

            mParameterBindings.push_back(parameterBinding);
        }

        mValid = buildResult.Errors.empty();
        buildResult.Valid = mValid;

        return buildResult;

        
    }

    void XJMaterialParameterLayout::Clear()
    {
        mValid = false;
        mUboSize = 0;
        mUboName.clear();
        mParameterBindings.clear();
        mTextureBindings.clear();
    }

    const XJMaterialParameterBinding* XJMaterialParameterLayout::FindParameterBinding(const std::string& parameterName) const
    {
        for (const auto& binding : mParameterBindings)
        {
            if (binding.ParameterName == parameterName)
                return &binding;
        }
        return nullptr;
    }

    const XJMaterialTextureBinding* XJMaterialParameterLayout::FindTextureBinding(const std::string& parameterName) const
    {
        for (const auto& binding : mTextureBindings)
        {
            if (binding.ParameterName == parameterName)
                return &binding;
        }

        return nullptr;
    }


}
